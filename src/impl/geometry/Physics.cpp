#include <glm/glm.hpp>
#include <tracy/Tracy.hpp>
#include <math.h>

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

static void applyGlobalForces(PhysicsWorld& World, float)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	for (ObjectHandle& object : World.Dynamics)
	{
		EcRigidBody* crb = object->FindComponent<EcRigidBody>();
		assert(crb->Mass == crb->CollisionAabb.Size.x * crb->CollisionAabb.Size.y * crb->CollisionAabb.Size.z * crb->Density);

		const glm::vec3 Gfs = { 0.f, -50.f, 0.f };

		// 19/09/2024 https://www.youtube.com/watch?v=-_IspRG548E
		glm::vec3 weight = Gfs * crb->Mass;

		const float AirDensity = 0.15f;
		const float DragCoefficient = 0.01f;
		float csarea = crb->CollisionAabb.Size.x * crb->CollisionAabb.Size.z;
		glm::vec3 drag = .5f * AirDensity * (crb->LinearVelocity * crb->LinearVelocity) * DragCoefficient * csarea;

		crb->NetForce = weight + drag;
	}
}

static void moveDynamics(PhysicsWorld& World, float DeltaTime)
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

	for (float x = min.x; x <= max.x; x++)
		for (float y = min.y; y <= max.y; y++)
			for (float z = min.z; z <= max.z; z++)
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

static void resolveCollisions(PhysicsWorld& World, float)
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
		glm::vec3 max = aPos - aSize / 2.f;
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
		// 24/09/2024
		// ugly
		// (another ugly at the end of this cycle)
		// Do this because otherwise, objects colliding will
		// move twice as fast
		//std::vector<Object_Base3D*> us = { collision.A, collision.B };
		//moveDynamics(us, -DeltaTime);

		// this much velocity is lost completely, pushing on neither objects
		float Elasticity = 0.2f;

		const IntersectionLib::CollisionPoints& points = collision.Points;

		glm::vec3 reactionForce = points.Normal * points.PenetrationDepth * Elasticity;

		EcRigidBody* arb = collision.A->FindComponent<EcRigidBody>();
		EcRigidBody* brb = collision.B->FindComponent<EcRigidBody>();
		EcTransform* at = collision.A->FindComponent<EcTransform>();

		if (arb->PhysicsDynamics && !brb->PhysicsDynamics)
		{
			arb->LinearVelocity = glm::vec3();

			// "Friction"
			arb->NetForce -= arb->LinearVelocity * arb->Friction * at->Size * (glm::vec3(1.f) - points.Normal);
			arb->NetForce *= glm::vec3(1.f) - points.Normal; // vertical forces cancel out

			arb->LinearVelocity = arb->LinearVelocity * (glm::vec3(1.f) - points.Normal) - arb->LinearVelocity * (points.Normal * Elasticity);

			if (-points.PenetrationDepth < 0.01f)
			{
				arb->LinearVelocity *= glm::vec3(1.f) - points.Normal;
				at->Transform[3] += glm::vec4(glm::vec3(points.Normal * -points.PenetrationDepth), 1.f);
			}

			at->SetWorldTransform(at->Transform);

			arb->RecomputeAabb();
		}
		else
		{
			// Transfer of velocity
			//collision.A->LinearVelocity += collision.B->LinearVelocity / 2.f;
			//collision.B->LinearVelocity += collision.A->LinearVelocity / 2.f;
			//collision.A->LinearVelocity = collision.A->LinearVelocity / 2.f;
			//collision.B->LinearVelocity = collision.B->LinearVelocity / 2.f;

			arb->LinearVelocity += reactionForce;

			arb->RecomputeAabb();
		}
		assert(arb->PhysicsDynamics); // `A` should always be the dynamic one, unless it's D v D where both are dynamic

		// 24/09/2024
		// ugly
		
		//moveDynamics(us, DeltaTime);
	}
}

static void step(PhysicsWorld& World, float DeltaTime)
{
	TIME_SCOPE_AS("Physics");
	ZoneScopedC(tracy::Color::AntiqueWhite);

	applyGlobalForces(World, DeltaTime);

	resolveCollisions(World, DeltaTime);

	moveDynamics(World, DeltaTime);
}

void Physics::Step(PhysicsWorld& World, double DeltaTime)
{
	step(World, std::clamp(static_cast<float>(DeltaTime), 0.f, 1.f/30.f));

	/*
	static double MaximumDeltaTime = 1.f / 240.f;

	if (DeltaTime <= MaximumDeltaTime)
		step(World, DeltaTime);
	else
	{
		double timeRemaining = DeltaTime;

		uint32_t numMiniSteps = 0;

		static uint32_t MaxMiniSteps = 32;

		while (timeRemaining > MaximumDeltaTime && numMiniSteps < MaxMiniSteps)
		{
			numMiniSteps++;

			double miniStep = timeRemaining - MaximumDeltaTime;
			timeRemaining -= miniStep;

			step(World, miniStep);
		}

		step(World, MaximumDeltaTime);
	}
	*/
}
