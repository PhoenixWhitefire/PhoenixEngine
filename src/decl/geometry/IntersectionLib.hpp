#pragma once

#include <glm/matrix.hpp>

#include "component/RigidBody.hpp"

namespace IntersectionLib
{
	struct Intersection
	{
		bool Occurred = false;

		glm::vec3 Position;
		glm::vec3 Normal;
		float Depth;
		float Time; // only used in `::RayAabb`
	};

	struct SweptIntersection
	{
		glm::vec3 Position;
		Intersection Hit;
		float Time;
	};

	struct CollisionPoints
	{
		glm::vec3 A; // Contact point (or furthest point) of A into B
		glm::vec3 B; // Contact point (or furthest point) of B into A
		glm::vec3 Normal;
		float PenetrationDepth;
		bool HasCollision = false;
	};

	Intersection AabbAabb(
		const glm::vec3& APosition,
		const glm::vec3& ASize,
		const glm::vec3& BPosition,
		const glm::vec3& BSize
	);

	Intersection RayAabb(
		const glm::vec3& Origin,
		const glm::vec3& Vector,
		const glm::vec3& BbPosition,
		const glm::vec3& BbSize,
		const glm::vec3& Padding = glm::vec3(1.f)
	);

	SweptIntersection SweptAabbAabb(
		const glm::vec3& APosition,
		const glm::vec3& ASize,
		const glm::vec3& BPosition,
		const glm::vec3& BSize,
		const glm::vec3& Delta
	);

	CollisionPoints Gjk(const EcRigidBody* A, const EcRigidBody* B);
}
