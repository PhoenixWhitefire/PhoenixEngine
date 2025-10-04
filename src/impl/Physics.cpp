#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "Physics.hpp"
#include "IntersectionLib.hpp"
#include "Timing.hpp"
#include "component/Transform.hpp"
#include "component/Mesh.hpp"

struct Collision
{
	GameObject* A;
	GameObject* B;
	IntersectionLib::Intersection Hit;
};

static void applyGlobalForces(Memory::vector<ObjectHandle, MEMCAT(Physics)>& World, float DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	for (ObjectHandle& object : World)
	{
		EcMesh* cm = object->GetComponent<EcMesh>();
		EcTransform* ct = object->GetComponent<EcTransform>();
		assert(ct);

		cm->Mass = cm->Density * ct->Size.x * ct->Size.y * ct->Size.z;

		if (cm->PhysicsDynamics)
		{
			static glm::vec3 GravityStrength{ 0.f, -50.f, 0.f };

			// 19/09/2024 https://www.youtube.com/watch?v=-_IspRG548E
			glm::vec3 force = GravityStrength * static_cast<float>(cm->Mass);

			static const float AirResistance = 0.15f;

			cm->LinearVelocity = cm->LinearVelocity - (cm->LinearVelocity * AirResistance * DeltaTime);
			cm->LinearVelocity += force / static_cast<float>(cm->Mass) * DeltaTime;
		}
	}
}

static void moveDynamics(Memory::vector<ObjectHandle, MEMCAT(Physics)>& World, float DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	for (ObjectHandle& object : World)
		if (EcMesh* cm = object->GetComponent<EcMesh>(); cm->PhysicsDynamics)
		{
			EcTransform* ct = object->GetComponent<EcTransform>();

			const glm::vec3& curPos = ct->Transform[3];
			glm::vec3 newPos = curPos + (glm::vec3)(cm->LinearVelocity * DeltaTime);
			ct->Transform[3] = glm::vec4(newPos, ct->Transform[3][3]);

			cm->RecomputeAabb();
		}
}

static void resolveCollisions(Memory::vector<ObjectHandle, MEMCAT(Physics)>& World, float DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	Memory::vector<Collision, MEMCAT(Physics)> collisions;

	for (ObjectHandle& a : World)
	{
		EcMesh* am = a->GetComponent<EcMesh>();

		if (!am->PhysicsCollisions || !am->PhysicsDynamics)
			continue;

		const glm::vec3& aPos = am->CollisionAabb.Position;
		const glm::vec3& aSize = am->CollisionAabb.Size;

		for (ObjectHandle& b : World)
		{
			if (a == b)
				continue;

			EcMesh* bm = b->GetComponent<EcMesh>();

			if (!bm->PhysicsCollisions)
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
		static const float Elasticity = 0.5f;

		IntersectionLib::Intersection& hit = collision.Hit;

		glm::vec3 reactionForce = hit.Position * hit.Depth;

		EcMesh* am = collision.A->GetComponent<EcMesh>();
		EcMesh* bm = collision.B->GetComponent<EcMesh>();
		EcTransform* at = collision.A->GetComponent<EcTransform>();

		if (am->PhysicsDynamics && !bm->PhysicsDynamics)
		{
			// "Friction"
			am->LinearVelocity = am->LinearVelocity - (am->LinearVelocity * static_cast<float>(am->Friction) * DeltaTime);

			double dot = glm::dot(am->LinearVelocity, hit.Normal);
			glm::vec3 velCoefficient{ 1.f, 1.f, 1.f };

			if (dot < 0.f)
				velCoefficient = glm::vec3(1.f) - glm::abs(hit.Normal) * (2.f * Elasticity);

			am->LinearVelocity = am->LinearVelocity * velCoefficient + reactionForce;
			at->Transform[3] += glm::vec4(glm::vec3(hit.Position * hit.Depth), 1.f);

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

		// 24/09/2024
		// ugly
		
		//moveDynamics(us, DeltaTime);
	}
}

static void step(Memory::vector<ObjectHandle, MEMCAT(Physics)>& World, float DeltaTime)
{
	TIME_SCOPE_AS("Physics");
	ZoneScopedC(tracy::Color::AntiqueWhite);

	applyGlobalForces(World, DeltaTime);

	moveDynamics(World, DeltaTime);

	resolveCollisions(World, DeltaTime);
}

void Physics::Step(Memory::vector<ObjectHandle, MEMCAT(Physics)>& World, double DeltaTime)
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
