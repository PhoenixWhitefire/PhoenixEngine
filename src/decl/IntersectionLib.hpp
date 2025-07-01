#pragma once

#include <glm/matrix.hpp>

namespace IntersectionLib
{
	struct Intersection
	{
		bool Occurred = false;

		glm::vec3 Vector{};
		glm::vec3 Normal{};
		float Depth{};
	};

	struct SweptIntersection
	{
		glm::vec3 Position{};
		Intersection Hit{};
		float Time{};
	};

	IntersectionLib::Intersection AabbAabb(
		const glm::vec3& APosition,
		const glm::vec3& ASize,
		const glm::vec3& BPosition,
		const glm::vec3& BSize
	);

	IntersectionLib::Intersection LineAabb(
		const glm::vec3& Origin,
		const glm::vec3& Vector,
		const glm::vec3& BbPosition,
		const glm::vec3& BbSize,
		float PaddingX = 0.f,
		float PaddingY = 0.f,
		float PaddingZ = 0.f
	);

	IntersectionLib::SweptIntersection SweepAabb(
		const glm::vec3& APosition,
		const glm::vec3& ASize,
		const glm::vec3& BPosition,
		const glm::vec3& BSize,
		const glm::vec3& Delta
	);
}
