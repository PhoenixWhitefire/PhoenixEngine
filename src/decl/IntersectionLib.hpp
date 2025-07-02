#pragma once

#include <glm/matrix.hpp>

namespace IntersectionLib
{
	struct Intersection
	{
		bool Occurred = false;

		glm::vec3 Position{};
		glm::vec3 Normal{};
		float Depth{};
		float Time{}; // only used in `::LineAabb`
	};

	struct SweptIntersection
	{
		glm::vec3 Position{};
		Intersection Hit{};
		float Time{};
	};

	Intersection AabbAabb(
		const glm::vec3& APosition,
		const glm::vec3& ASize,
		const glm::vec3& BPosition,
		const glm::vec3& BSize
	);

	Intersection LineAabb(
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
}
