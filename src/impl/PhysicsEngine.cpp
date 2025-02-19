#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "PhysicsEngine.hpp"
#include "IntersectionLib.hpp"
#include "PerformanceTiming.hpp"

struct Collision
{
	Object_Base3D* A;
	Object_Base3D* B;
	IntersectionLib::Intersection Hit;
};

static void applyGlobalForces(std::vector<Object_Base3D*>& World, double DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	for (Object_Base3D* object : World)
	{
		object->Mass = object->Density * object->Size.X * object->Size.Y * object->Size.Z;

		if (object->PhysicsDynamics)
		{
			static Vector3 GravityStrength{ 0.f, -50.f, 0.f };

			// 19/09/2024 https://www.youtube.com/watch?v=-_IspRG548E
			Vector3 force = GravityStrength * object->Mass;

			static const double AirResistance = 0.15f;

			object->LinearVelocity = object->LinearVelocity - (object->LinearVelocity * AirResistance * DeltaTime);
			object->LinearVelocity += force / object->Mass * DeltaTime;
		}
	}
}

static void moveDynamics(std::vector<Object_Base3D*>& World, double DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	for (Object_Base3D* object : World)
		if (object->PhysicsDynamics)
		{
			const glm::vec3& curPos = object->Transform[3];
			glm::vec3 newPos = curPos + (glm::vec3)(object->LinearVelocity * DeltaTime);
			object->Transform[3] = glm::vec4(newPos, object->Transform[3][3]);

			object->RecomputeAabb();
		}
}

static void resolveCollisions(std::vector<Object_Base3D*>& World, double DeltaTime)
{
	ZoneScopedC(tracy::Color::AntiqueWhite);

	std::vector<Collision> collisions;

	for (Object_Base3D* a : World)
	{
		if (!a->PhysicsCollisions)
			continue;

		const glm::vec3& aPos = a->CollisionAabb.Position;
		const glm::vec3& aSize = a->CollisionAabb.Size;

		for (Object_Base3D* b : World)
		{
			if (a == b)
				continue;

			if (!b->PhysicsCollisions)
				continue;

			if (!a->PhysicsDynamics && !b->PhysicsDynamics)
				continue;

			const glm::vec3& bPos = b->CollisionAabb.Position;
			const glm::vec3& bSize = b->CollisionAabb.Size;

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

		if (collision.A->PhysicsDynamics && !collision.B->PhysicsDynamics)
		{
			// "Friction"
			collision.A->LinearVelocity = collision.A->LinearVelocity - (collision.A->LinearVelocity * collision.A->Friction * DeltaTime);

			double dot = collision.A->LinearVelocity.Dot(hit.Normal);
			Vector3 velCoefficient = Vector3::one;

			if (dot < 0.f)
				velCoefficient = Vector3::one - Vector3(hit.Normal).Abs() * (2.f * Elasticity);

			collision.A->LinearVelocity = collision.A->LinearVelocity * velCoefficient + reactionForce;
			collision.A->Transform[3] += glm::vec4(glm::vec3(hit.Vector * hit.Depth), 1.f);

			collision.A->RecomputeAabb();
		}
		else if (collision.B->PhysicsDynamics && !collision.A->PhysicsDynamics)
		{
			collision.B->LinearVelocity = collision.B->LinearVelocity - (collision.B->LinearVelocity * collision.B->Friction * DeltaTime);

			double dot = collision.A->LinearVelocity.Dot(hit.Normal);
			Vector3 velCoefficient = Vector3::one;

			if (dot < 0.f)
				velCoefficient = Vector3::one - Vector3(hit.Normal).Abs() * (2.f * Elasticity);

			collision.B->LinearVelocity = collision.B->LinearVelocity * velCoefficient + reactionForce;
			collision.B->Transform[3] += glm::vec4(glm::vec3(hit.Vector * hit.Depth), 1.f);

			collision.B->RecomputeAabb();
		}
		else
		{
			// Transfer of velocity
			//collision.A->LinearVelocity += collision.B->LinearVelocity / 2.f;
			//collision.B->LinearVelocity += collision.A->LinearVelocity / 2.f;
			//collision.A->LinearVelocity = collision.A->LinearVelocity / 2.f;
			//collision.B->LinearVelocity = collision.B->LinearVelocity / 2.f;

			collision.A->LinearVelocity += reactionForce;
			collision.B->LinearVelocity += reactionForce;

			collision.A->RecomputeAabb();
			collision.B->RecomputeAabb();
		}

		// 24/09/2024
		// ugly
		
		//moveDynamics(us, DeltaTime);
	}
}

static void step(std::vector<Object_Base3D*>& World, double DeltaTime)
{
	TIME_SCOPE_AS(Timing::Timer::Physics);
	ZoneScopedC(tracy::Color::AntiqueWhite);

	applyGlobalForces(World, DeltaTime);

	moveDynamics(World, DeltaTime);

	resolveCollisions(World, DeltaTime);
}

void Physics::Step(std::vector<Object_Base3D*>& World, double DeltaTime)
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
