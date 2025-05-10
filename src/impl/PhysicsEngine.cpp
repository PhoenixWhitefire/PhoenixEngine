#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "PhysicsEngine.hpp"
#include "IntersectionLib.hpp"
#include "PerformanceTiming.hpp"
#include "component/Transform.hpp"
#include "component/Mesh.hpp"

struct Collision
{
	GameObject* A;
	GameObject* B;
	IntersectionLib::Intersection Hit;
};

static void applyGlobalForces(std::vector<GameObject*>& World, double DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	for (GameObject* object : World)
	{
		EcMesh* cm = object->GetComponent<EcMesh>();
		EcTransform* ct = object->GetComponent<EcTransform>();
		assert(ct);

		cm->Mass = cm->Density * ct->Size.X * ct->Size.Y * ct->Size.Z;

		if (cm->PhysicsDynamics)
		{
			static Vector3 GravityStrength{ 0.f, -50.f, 0.f };

			// 19/09/2024 https://www.youtube.com/watch?v=-_IspRG548E
			Vector3 force = GravityStrength * cm->Mass;

			static const double AirResistance = 0.15f;

			cm->LinearVelocity = cm->LinearVelocity - (cm->LinearVelocity * AirResistance * DeltaTime);
			cm->LinearVelocity += force / cm->Mass * DeltaTime;
		}
	}
}

static void moveDynamics(std::vector<GameObject*>& World, double DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	for (GameObject* object : World)
		if (EcMesh* cm = object->GetComponent<EcMesh>(); cm->PhysicsDynamics)
		{
			EcTransform* ct = object->GetComponent<EcTransform>();

			const glm::vec3& curPos = ct->Transform[3];
			glm::vec3 newPos = curPos + (glm::vec3)(cm->LinearVelocity * DeltaTime);
			ct->Transform[3] = glm::vec4(newPos, ct->Transform[3][3]);

			cm->RecomputeAabb();
		}
}

static void resolveCollisions(std::vector<GameObject*>& World, double DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	std::vector<Collision> collisions;

	for (GameObject* a : World)
	{
		EcMesh* am = a->GetComponent<EcMesh>();

		if (!am->PhysicsCollisions)
			continue;

		const glm::vec3& aPos = am->CollisionAabb.Position;
		const glm::vec3& aSize = am->CollisionAabb.Size;

		for (GameObject* b : World)
		{
			if (a == b)
				continue;

			EcMesh* bm = b->GetComponent<EcMesh>();

			if (!bm->PhysicsCollisions)
				continue;

			if (!am->PhysicsDynamics && !bm->PhysicsDynamics)
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

		glm::vec3 reactionForce = hit.Vector * hit.Depth;

		EcMesh* am = collision.A->GetComponent<EcMesh>();
		EcMesh* bm = collision.B->GetComponent<EcMesh>();
		EcTransform* at = collision.A->GetComponent<EcTransform>();
		EcTransform* bt = collision.B->GetComponent<EcTransform>();

		if (am->PhysicsDynamics && !bm->PhysicsDynamics)
		{
			// "Friction"
			am->LinearVelocity = am->LinearVelocity - (am->LinearVelocity * am->Friction * DeltaTime);

			double dot = am->LinearVelocity.Dot(hit.Normal);
			Vector3 velCoefficient = Vector3::one;

			if (dot < 0.f)
				velCoefficient = Vector3::one - Vector3(hit.Normal).Abs() * (2.f * Elasticity);

			am->LinearVelocity = am->LinearVelocity * velCoefficient + reactionForce;
			at->Transform[3] += glm::vec4(glm::vec3(hit.Vector * hit.Depth), 1.f);

			am->RecomputeAabb();
		}
		else if (bm->PhysicsDynamics && !am->PhysicsDynamics)
		{
			bm->LinearVelocity = bm->LinearVelocity - (bm->LinearVelocity * bm->Friction * DeltaTime);

			double dot = am->LinearVelocity.Dot(hit.Normal);
			Vector3 velCoefficient = Vector3::one;

			if (dot < 0.f)
				velCoefficient = Vector3::one - Vector3(hit.Normal).Abs() * (2.f * Elasticity);

			bm->LinearVelocity = bm->LinearVelocity * velCoefficient + reactionForce;
			bt->Transform[3] += glm::vec4(glm::vec3(hit.Vector * hit.Depth), 1.f);

			bm->RecomputeAabb();
		}
		else
		{
			// Transfer of velocity
			//collision.A->LinearVelocity += collision.B->LinearVelocity / 2.f;
			//collision.B->LinearVelocity += collision.A->LinearVelocity / 2.f;
			//collision.A->LinearVelocity = collision.A->LinearVelocity / 2.f;
			//collision.B->LinearVelocity = collision.B->LinearVelocity / 2.f;

			am->LinearVelocity += reactionForce;
			bm->LinearVelocity += reactionForce;

			am->RecomputeAabb();
			bm->RecomputeAabb();
		}

		// 24/09/2024
		// ugly
		
		//moveDynamics(us, DeltaTime);
	}
}

static void step(std::vector<GameObject*>& World, double DeltaTime)
{
	TIME_SCOPE_AS(Timing::Timer::Physics);
	ZoneScopedC(tracy::Color::AntiqueWhite);

	applyGlobalForces(World, DeltaTime);

	moveDynamics(World, DeltaTime);

	resolveCollisions(World, DeltaTime);
}

void Physics::Step(std::vector<GameObject*>& World, double DeltaTime)
{
	step(World, std::clamp(DeltaTime, (double)0.f, (double)1.f/30.f));

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
