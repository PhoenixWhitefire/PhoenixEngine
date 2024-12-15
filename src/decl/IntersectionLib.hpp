#pragma once

#include <glm/matrix.hpp>
#include <vector>

#include "datatype/Vector3.hpp"

namespace IntersectionLib
{
	struct Intersection
	{
		bool Occurred = false;

		Vector3 Vector{};
		Vector3 Normal{};
		double Depth{};
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
}
