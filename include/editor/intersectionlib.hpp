#pragma once

#include<glm/matrix.hpp>
#include<datatype/Vector3.hpp>
#include<vector>

#include<datatype/Mesh.hpp>

namespace IntersectionLib
{
	struct IntersectionResult
	{
		bool DidHit = false;
		Vector3 HitPosition;
		unsigned int HitObjectId = 0;
	};

	struct Triangle
	{
		Vector3 P1;
		Vector3 P2;
		Vector3 P3;
	};

	struct HittableObject
	{
		Mesh* CollisionMesh = nullptr;
		glm::mat4 Matrix = glm::mat4(1.0f);
		unsigned int Id = 0;
	};

	IntersectionResult Traceline(Vector3 LineStart, Vector3 LineEnd, std::vector<HittableObject*> Objects);
}
