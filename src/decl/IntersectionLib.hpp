#pragma once

#include <glm/matrix.hpp>
#include <vector>

#include "gameobject/Base3D.hpp"

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
		glm::vec3& APosition,
		glm::vec3& ASize,
		glm::vec3& BPosition,
		glm::vec3& BSize
	);

	IntersectionLib::Intersection LineAabb(
		glm::vec3& Origin,
		glm::vec3& Vector,
		glm::vec3& BbPosition,
		glm::vec3& BbSize,
		float PaddingX = 0.f,
		float PaddingY = 0.f,
		float PaddingZ = 0.f
	);
}
