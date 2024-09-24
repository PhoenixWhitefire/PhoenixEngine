#include<glm/gtc/matrix_transform.hpp>

#include"PhysicsEngine.hpp"

struct CollisionPoints
{
	Vector3 A; // Furthest point of A into B
	Vector3 B; // Furthest point of B into A
	Vector3 Normal; // B - A normalized
	double Depth; // Length of B - A
	bool HasCollision;
};

struct Collision
{
	Object_Base3D* A;
	Object_Base3D* B;
	CollisionPoints Points;
};

static int sign(double v)
{
	return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

static void moveDynamics(std::vector<Object_Base3D*>& World, double DeltaTime)
{
	for (Object_Base3D* object : World)
		if (object->PhysicsDynamics)
		{
			const glm::vec3& curPos = object->Transform[3];
			glm::vec3 newPos = curPos + (glm::vec3)(object->LinearVelocity * DeltaTime);
			object->Transform[3] = glm::vec4(newPos, object->Transform[3][3]);
		}
}

static void resolveCollisions(std::vector<Object_Base3D*>& World, double DeltaTime)
{
	std::vector<Collision> collisions;

	for (Object_Base3D* a : World)
	{
		if (!a->PhysicsCollisions)
			continue;

		Vector3 aPos = glm::vec3(a->Transform[3]);
		Vector3 aMin = aPos - a->Size;
		Vector3 aMax = aPos + a->Size;
		Vector3 aSizeHalf = a->Size / 2.f;

		for (Object_Base3D* b : World)
		{
			if (a == b)
				continue;
			if (!b->PhysicsCollisions)
				continue;
			if (!a->PhysicsDynamics && !b->PhysicsDynamics)
				continue;

			Vector3 bPos = glm::vec3(b->Transform[3]);
			Vector3 bMin = bPos - b->Size;
			Vector3 bMax = bPos + b->Size;
			Vector3 bSizeHalf = b->Size / 2.f;

			// https://noonat.github.io/intersect/
			double dx = bPos.X - aPos.X;
			double px = (bSizeHalf.X + aSizeHalf.X) - std::abs(dx);
			if (px <= 0)
				continue;

			double dy = bPos.Y - aPos.Y;
			double py = (bSizeHalf.Y + aSizeHalf.Y) - std::abs(dy);
			if (py <= 0)
				continue;

			double dz = bPos.Z - aPos.Z;
			double pz = (bSizeHalf.Z + aSizeHalf.Z) - std::abs(dz);
			if (pz <= 0)
				continue;

			CollisionPoints points{};
			points.HasCollision = true;

			if (px < py && px < pz)
			{
				int sx = sign(dx);
				points.Depth = px * sx;
				points.Normal = Vector3(sx, 0.f, 0.f);
				points.A = Vector3(aPos.X + (aSizeHalf.X * sx), 0.f, 0.f);
			}
			else if (py < px && py < pz)
			{
				int sy = sign(dy);
				points.Depth = py * sy;
				points.Normal = Vector3(0.f, sy, 0.f);
				points.A = Vector3(0.f, aPos.Y + (aSizeHalf.Y * sy), 0.f);
			}
			else if (pz < py)
			{
				int sz = sign(dz);
				points.Depth = pz * sz;
				points.Normal = Vector3(0.f, 0.f, sz);
				points.A = Vector3(0.f, 0.f, aPos.X + (aSizeHalf.Z * sz));
			}

			collisions.emplace_back(a, b, points);
		}
	}

	for (Collision& collision : collisions)
	{
		// 24/09/2024
		// ugly
		// (another ugly at the end of this cycle)
		std::vector<Object_Base3D*> us = { collision.A, collision.B };
		moveDynamics(us, -DeltaTime);

		CollisionPoints& points = collision.Points;

		// this much velocity is lost completely, pushing on neither objects
		static const float Elasticity = 0.8f;

		Vector3 reactionForce = points.A * points.Depth;

		if (collision.A->PhysicsDynamics && !collision.B->PhysicsDynamics)
		{
			// "Friction"
			collision.A->LinearVelocity = collision.A->LinearVelocity - (collision.A->LinearVelocity * collision.A->Friction * DeltaTime);

			collision.A->LinearVelocity = collision.A->LinearVelocity * (Vector3::one - points.Normal) + reactionForce;
		}
		else if (collision.B->PhysicsDynamics && !collision.A->PhysicsDynamics)
		{
			collision.B->LinearVelocity = collision.B->LinearVelocity - (collision.B->LinearVelocity * collision.B->Friction * DeltaTime);

			collision.B->LinearVelocity = collision.B->LinearVelocity * (Vector3::one - points.Normal) + reactionForce;
		}
		else
		{
			// Transfer of velocity
			collision.A->LinearVelocity += collision.B->LinearVelocity / 2.f;
			collision.B->LinearVelocity += collision.A->LinearVelocity / 2.f;
			collision.A->LinearVelocity = collision.A->LinearVelocity / 2.f;
			collision.B->LinearVelocity = collision.B->LinearVelocity / 2.f;

			collision.A->LinearVelocity += reactionForce / 2.f;
			collision.B->LinearVelocity -= reactionForce / 2.f;
		}

		// 24/09/2024
		// ugly
		
		moveDynamics(us, DeltaTime);
	}
}

void Physics::Step(std::vector<Object_Base3D*>& World, double DeltaTime)
{
	for (Object_Base3D* object : World)
	{
		object->Mass = object->Density * object->Size.X * object->Size.Y * object->Size.Z;

		if (object->PhysicsDynamics)
		{
			static Vector3 GravityStrength{ 0.f, -50.f, 0.f };

			// 19/09/2024 https://www.youtube.com/watch?v=-_IspRG548E
			Vector3 force = GravityStrength * object->Mass;

			static const double GlobalResistance = 0.15f;

			object->LinearVelocity = object->LinearVelocity - (object->LinearVelocity * GlobalResistance * DeltaTime);
			object->LinearVelocity += force / object->Mass * DeltaTime;
		}
	}

	moveDynamics(World, DeltaTime);

	resolveCollisions(World, DeltaTime);
}
