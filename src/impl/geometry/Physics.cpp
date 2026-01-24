#include <glm/glm.hpp>
#include <tracy/Tracy.hpp>
#include <math.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/orthonormalize.hpp>

#include "geometry/Physics.hpp"
#include "geometry/IntersectionLib.hpp"
#include "Timing.hpp"
#include "Stl.hpp"
#include "component/Transform.hpp"
#include "component/Workspace.hpp"
#include "component/RigidBody.hpp"

struct Collision
{
	GameObject* A;
	GameObject* B;
	IntersectionLib::CollisionPoints Points;
};

static Physics* Instance;

Physics::Physics()
{
	assert(!Instance);
	Instance = this;
}

Physics::~Physics()
{
	assert(Instance);
	Instance = nullptr;
}

Physics* Physics::Get()
{
	assert(Instance);
	return Instance;
}

static void applyGlobalForces(Physics::World& World, float, Physics* phys)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	for (ObjectHandle& object : World.Dynamics)
	{
		EcRigidBody* crb = object->FindComponent<EcRigidBody>();
		assert(crb->Mass == crb->CollisionAabb.Size.x * crb->CollisionAabb.Size.y * crb->CollisionAabb.Size.z * crb->Density);

		// 19/09/2024 https://www.youtube.com/watch?v=-_IspRG548E
		glm::vec3 weight = phys->Gravity * crb->Mass;

		const float AirDensity = 0.15f;
		const float DragCoefficient = 0.01f;
		float csarea = crb->CollisionAabb.Size.x * crb->CollisionAabb.Size.z;

		glm::vec3 v = crb->LinearVelocity;
		float speed = glm::length(v);

		glm::vec3 drag = speed > 0.f
		    ? -glm::normalize(v) * 0.5f * AirDensity * speed * speed * DragCoefficient * csarea
		    : glm::vec3(0.f);

		crb->NetForce = weight + drag;
	}
}

static void moveDynamics(Physics::World& World, float DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	for (ObjectHandle& object : World.Dynamics)
	{
		EcRigidBody* crb = object->FindComponent<EcRigidBody>();
		EcTransform* ct = object->FindComponent<EcTransform>();

		glm::vec3 acceleration = crb->NetForce / crb->Mass;
		crb->LinearVelocity += acceleration * DeltaTime;
		if (!isfinite(crb->LinearVelocity.x) || !isfinite(crb->LinearVelocity.y) || !isfinite(crb->LinearVelocity.z) || glm::length(crb->LinearVelocity) > 10000.f)
			crb->LinearVelocity = glm::vec3(0.f);

		glm::mat4 curTrans = ct->Transform;
		curTrans[3] += glm::vec4(crb->LinearVelocity * DeltaTime, 0.f);

		if (!isfinite(curTrans[3].x) || !isfinite(curTrans[3].y) || !isfinite(curTrans[3].z))
			curTrans[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);

		const glm::vec3& w = crb->AngularVelocity;

		glm::mat3 skew = glm::mat3(
			0,   -w.z, w.y,
			w.z,  0,  -w.x,
			-w.y, w.x, 0
		);

		glm::mat3 rot = glm::mat3(curTrans);
		rot += skew * rot * DeltaTime;
		rot = glm::orthonormalize(rot);

		glm::vec3 pos = glm::vec3(curTrans[3]);

		curTrans = glm::mat4(rot);
		curTrans[3] = glm::vec4(pos, 1.f);

		ct->SetWorldTransform(curTrans);
		crb->RecomputeAabb();
	}
}

static int roundNToGrid(float x)
{
	return int(glm::round(x / SPATIAL_HASH_GRID_SIZE) * SPATIAL_HASH_GRID_SIZE);
}

static glm::ivec3 roundToGrid(const glm::vec3& v)
{
	return glm::ivec3(roundNToGrid(v.x), roundNToGrid(v.y), roundNToGrid(v.z));
}

typedef std::unordered_map<glm::ivec3, std::vector<uint32_t>>::iterator VisitIterator;

static void visitHashAabb(EcWorkspace* cw, const glm::vec3& min, const glm::vec3& max, const std::function<bool(VisitIterator)> Visit)
{
	if (abs(min.x) + abs(min.y) + abs(min.z) > 1e6)
		return;

	for (float x = min.x; x <= max.x; x += SPATIAL_HASH_GRID_SIZE)
		for (float y = min.y; y <= max.y; y += SPATIAL_HASH_GRID_SIZE)
			for (float z = min.z; z <= max.z; z += SPATIAL_HASH_GRID_SIZE)
			{
				glm::ivec3 point = { x, y, z };
				assert(roundToGrid(point) == point);

				const auto& it = cw->SpatialHash.find(point);
				if (it == cw->SpatialHash.end())
					continue;

				if (Visit(it)) // stop condition
					return;
			}
}

#include "Engine.hpp"

static void resolveCollisions(Physics::World& World, float DeltaTime, Physics* phys)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	hx::vector<Collision, MEMCAT(Physics)> collisions;

	EcWorkspace* workspace = GameObject::GetObjectById(World.Dynamics[0]->OwningWorkspace)->FindComponent<EcWorkspace>();

	for (size_t aid = 0; aid < World.Dynamics.size(); aid++)
	{
		ObjectHandle& a = World.Dynamics[aid];;
		EcRigidBody* arb = a->FindComponent<EcRigidBody>();

		if (!arb->PhysicsCollisions)
			continue;

		arb->CurTransform = a->FindComponent<EcTransform>();
		const glm::vec3& aPos = arb->CollisionAabb.Position;
		const glm::vec3& aSize = arb->CollisionAabb.Size;

		glm::vec3 min = aPos - aSize / 2.f;
		glm::vec3 max = aPos + aSize / 2.f;
		min = roundToGrid(min);
		max = roundToGrid(max);

		visitHashAabb(workspace, min, max, [&](VisitIterator it) -> bool
		{
			for (uint32_t oid : it->second)
			{
				if (oid == a->ObjectId)
					continue;

				GameObject* b = GameObject::GetObjectById(oid);
				EcRigidBody* brb = b ? b->FindComponent<EcRigidBody>() : nullptr;

				if (!brb || !brb->PhysicsCollisions)
					continue;

				brb->CurTransform = b->FindComponent<EcTransform>();

				IntersectionLib::CollisionPoints collisionPoints = IntersectionLib::Gjk(arb, brb);

				if (collisionPoints.HasCollision)
					collisions.emplace_back(a, b, collisionPoints);

				brb->CurTransform = nullptr;
			}

			return false; // process all collisions
		});

		arb->CurTransform = nullptr;
	}

	for (const Collision& collision : collisions)
	{
		const IntersectionLib::CollisionPoints& points = collision.Points;

		EcRigidBody* arb = collision.A->FindComponent<EcRigidBody>();
		EcRigidBody* brb = collision.B->FindComponent<EcRigidBody>();
		EcTransform* at = collision.A->FindComponent<EcTransform>();

		glm::vec3 vRel = arb->LinearVelocity - (brb->PhysicsDynamics ? brb->LinearVelocity : glm::vec3(0.f));
		float vn = glm::dot(vRel, points.Normal);

		if (arb->PhysicsDynamics && !brb->PhysicsDynamics)
		{
			if (vn > 0.f)
			{
			    float j = -(1.f + arb->Restitution) * vn;
			    j /= (1.f / arb->Mass);

			    arb->LinearVelocity += (j / arb->Mass) * points.Normal;

				if (arb->PhysicsRotations)
				{
					glm::vec3 r = points.A - arb->CollisionAabb.Position; // lever arm
					arb->AngularVelocity += glm::cross(r, points.Normal) * points.PenetrationDepth * 10.f;
				}
			}

			// position correction to prevent sinking
			if (points.PenetrationDepth < 0.1f)
				at->Transform[3] += glm::vec4(points.Normal * -points.PenetrationDepth * 2.f, 0.f);;

			arb->NetForce *= glm::vec3(1.f) - points.Normal;
			arb->NetForce -= arb->LinearVelocity * (glm::vec3(1.f) - points.Normal) * arb->Friction;

			arb->AngularVelocity -= arb->AngularVelocity * arb->Friction * 0.1f * DeltaTime;

			at->SetWorldTransform(at->Transform);
			arb->RecomputeAabb();

			if (phys->DebugContactPoints)
			{
				Engine::Get()->CurrentScene.RenderList.push_back(RenderItem{
					.RenderMeshId = 0,
					.Transform = glm::translate(glm::mat4(1.f), points.B),
					.Size = glm::vec3(0.2f),
					.MaterialId = MaterialManager::Get()->LoadFromPath("unlit"),
					.TintColor = glm::vec3(1.f, 0.f, 0.f),
					.Transparency = 0.1f,
					.FaceCulling = FaceCullingMode::None
				});
			}
		}
		else // Not finished yet
		{
			// Transfer of velocity
			//collision.A->LinearVelocity += collision.B->LinearVelocity / 2.f;
			//collision.B->LinearVelocity += collision.A->LinearVelocity / 2.f;
			//collision.A->LinearVelocity = collision.A->LinearVelocity / 2.f;
			//collision.B->LinearVelocity = collision.B->LinearVelocity / 2.f;

			//arb->LinearVelocity += reactionForce;

			arb->RecomputeAabb();
		}
		assert(arb->PhysicsDynamics); // `A` should always be the dynamic one, unless it's D v D where both are dynamic
	}
}

static void step(Physics::World& World, float DeltaTime, Physics* phys)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	applyGlobalForces(World, DeltaTime, phys);

	resolveCollisions(World, DeltaTime, phys);

	moveDynamics(World, DeltaTime);
}

void Physics::Step(Physics::World& World, double DeltaTime)
{
	TIME_SCOPE_AS("Physics");
	ZoneScopedC(tracy::Color::AntiqueWhite);

	static double MaximumDeltaTime = 1.0 / 240.0;

	if (DeltaTime <= MaximumDeltaTime)
		step(World, DeltaTime, this);
	else
	{
		double timeRemaining = DeltaTime;

		static size_t MaxSubsteps = 16;
		size_t numSubsteps = 0;

		while (timeRemaining > MaximumDeltaTime && numSubsteps < MaxSubsteps)
		{
			numSubsteps++;

			double substep = MaximumDeltaTime;
			timeRemaining -= substep;

			step(World, substep, this);
		}

		step(World, std::clamp(timeRemaining, 0.0, MaximumDeltaTime), this);
	}
}
