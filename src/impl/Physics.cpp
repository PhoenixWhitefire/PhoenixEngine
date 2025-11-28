#include <glm/glm.hpp>
#include <tracy/Tracy.hpp>
#include <math.h>

#include "Physics.hpp"
#include "IntersectionLib.hpp"
#include "Timing.hpp"
#include "component/Transform.hpp"
#include "component/Workspace.hpp"
#include "component/Mesh.hpp"

struct Collision
{
	GameObject* A;
	GameObject* B;
	IntersectionLib::Intersection Hit;
};

static void applyGlobalForces(PhysicsWorld& World, float DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	for (ObjectHandle& object : World.Dynamics)
	{
		EcMesh* cm = object->FindComponent<EcMesh>();
		assert(cm->Mass == cm->CollisionAabb.Size.x * cm->CollisionAabb.Size.y * cm->CollisionAabb.Size.z * cm->Density);

		const glm::vec3 Gfs = { 0.f, -50.f, 0.f };

		// 19/09/2024 https://www.youtube.com/watch?v=-_IspRG548E
		glm::vec3 weight = Gfs * cm->Mass;

		const float AirDensity = 0.15f;
		const float DragCoefficient = 0.01f;
		float csarea = cm->CollisionAabb.Size.x * cm->CollisionAabb.Size.z;
		glm::vec3 drag = .5f * AirDensity * (cm->LinearVelocity * cm->LinearVelocity) * DragCoefficient * csarea;

		cm->NetForce = weight + drag;
	}
}

static void moveDynamics(PhysicsWorld& World, float DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	for (ObjectHandle& object : World.Dynamics)
	{
		EcMesh* cm = object->FindComponent<EcMesh>();
		EcTransform* ct = object->FindComponent<EcTransform>();

		glm::vec3 acceleration = cm->NetForce / cm->Mass;
		cm->LinearVelocity += acceleration * DeltaTime;
		if (!isfinite(cm->LinearVelocity.x) || !isfinite(cm->LinearVelocity.y) || !isfinite(cm->LinearVelocity.z) || glm::length(cm->LinearVelocity) > 10000.f)
			cm->LinearVelocity = glm::vec3(0.f);

		glm::mat4 curTrans = ct->Transform;
		curTrans[3] += glm::vec4(cm->LinearVelocity * DeltaTime, 0.f);

		if (!isfinite(curTrans[3].x) || !isfinite(curTrans[3].y) || !isfinite(curTrans[3].z))
			curTrans[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);

		ct->SetWorldTransform(curTrans);
		cm->RecomputeAabb();
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

static void resolveCollisions(PhysicsWorld& World, float DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	Memory::vector<Collision, MEMCAT(Physics)> collisions;

	EcWorkspace* workspace = GameObject::GetObjectById(World.Dynamics[0]->OwningWorkspace)->FindComponent<EcWorkspace>();

	for (size_t aid = 0; aid < World.Dynamics.size(); aid++)
	{
		ObjectHandle& a = World.Dynamics[aid];;
		EcMesh* am = a->FindComponent<EcMesh>();

		if (!am->PhysicsCollisions)
			continue;

		const glm::vec3& aPos = am->CollisionAabb.Position;
		const glm::vec3& aSize = am->CollisionAabb.Size;

		glm::vec3 min = aPos - aSize / 2.f;
		glm::vec3 max = aPos - aSize / 2.f;
		min = roundToGrid(min);
		max = roundToGrid(max);

		visitHashAabb(workspace, min, max, [&](VisitIterator it) -> bool
		{
			for (uint32_t oid : it->second)
			{
				GameObject* b = GameObject::GetObjectById(oid);
				EcMesh* bm = b ? b->FindComponent<EcMesh>() : nullptr;

				if (!bm)
					continue;

				const glm::vec3& bPos = bm->CollisionAabb.Position;
				const glm::vec3& bSize = bm->CollisionAabb.Size;

				IntersectionLib::Intersection intersection = IntersectionLib::AabbAabb(
					aPos,
					aSize,
					bPos,
					bSize
				);

				if (intersection.Occurred)
					collisions.emplace_back(a, b, intersection);
			}

			return false; // process all collisions
		});
	}

	for (Collision& collision : collisions)
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

		IntersectionLib::Intersection& hit = collision.Hit;

		glm::vec3 reactionForce = hit.Position * hit.Depth * Elasticity;

		EcMesh* am = collision.A->FindComponent<EcMesh>();
		EcMesh* bm = collision.B->FindComponent<EcMesh>();
		EcTransform* at = collision.A->FindComponent<EcTransform>();

		if (am->PhysicsDynamics && !bm->PhysicsDynamics)
		{
			// "Friction"
			am->NetForce -= 2 * am->Friction * at->Size * (glm::vec3(1.f) - hit.Normal);
			am->NetForce *= glm::vec3(1.f) - hit.Normal; // vertical forces cancel out

			am->LinearVelocity = am->LinearVelocity * (glm::vec3(1.f) - hit.Normal) - am->LinearVelocity * (hit.Normal * Elasticity);

			if (-hit.Depth < 0.01f)
			{
				am->LinearVelocity *= glm::vec3(1.f) - hit.Normal;
				at->Transform[3] += glm::vec4(glm::vec3(hit.Normal * -hit.Depth), 1.f);
			}

			at->SetWorldTransform(at->Transform);

			am->RecomputeAabb();
		}
		else
		{
			// Transfer of velocity
			//collision.A->LinearVelocity += collision.B->LinearVelocity / 2.f;
			//collision.B->LinearVelocity += collision.A->LinearVelocity / 2.f;
			//collision.A->LinearVelocity = collision.A->LinearVelocity / 2.f;
			//collision.B->LinearVelocity = collision.B->LinearVelocity / 2.f;

			am->LinearVelocity += reactionForce;

			am->RecomputeAabb();
		}
		assert(am->PhysicsDynamics); // `A` should always be the dynamic one, unless it's D v D where both are dynamic

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
