#include <algorithm>
#include <float.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <tracy/Tracy.hpp>

#include "geometry/IntersectionLib.hpp"
#include "geometry/Gjk.hpp"

static const float EPSILON = 1e-8f;

template<class T> static int sign(T v)
{
	return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

IntersectionLib::Intersection IntersectionLib::AabbAabb
(
	const glm::vec3& APosition,
	const glm::vec3& ASize,
	const glm::vec3& BPosition,
	const glm::vec3& BSize
)
{
	Intersection result{};
	result.Occurred = false;

	glm::vec3 aSizeHalf = ASize * .5f;
	glm::vec3 bSizeHalf = BSize * .5f;

	// https://noonat.github.io/intersect/
	float dx = BPosition.x - APosition.x;
	float px = (bSizeHalf.x + aSizeHalf.x) - std::abs(dx);
	if (px <= 0.f)
		return result;

	float dy = BPosition.y - APosition.y;
	float py = (bSizeHalf.y + aSizeHalf.y) - std::abs(dy);
	if (py <= 0.f)
		return result;

	float dz = BPosition.z - APosition.z;
	float pz = (bSizeHalf.z + aSizeHalf.z) - std::abs(dz);
	if (pz <= 0.f)
		return result;

	result.Occurred = true;

	if (px < py && px < pz)
	{
		int sx = sign(dx);
		result.Depth = px * sx;
		result.Normal = glm::vec3(-sx, 0.f, 0.f);
		result.Position = glm::vec3(APosition.x + (aSizeHalf.x * sx), 0.f, 0.f);
	}
	else if (py < px && py < pz)
	{
		int sy = sign(dy);
		result.Depth = py * sy;
		result.Normal = glm::vec3(0.f, -sy, 0.f);
		result.Position = glm::vec3(0.f, APosition.y + (aSizeHalf.y * sy), 0.f);
	}
	else if (pz < py)
	{
		int sz = sign(dz);
		result.Depth = pz * sz;
		result.Normal = glm::vec3(0.f, 0.f, -sz);
		result.Position = glm::vec3(0.f, 0.f, APosition.z + (aSizeHalf.z * sz));
	}

	return result;
}

IntersectionLib::Intersection IntersectionLib::RayAabb(
	const glm::vec3& Origin,
	const glm::vec3& Vector,
	const glm::vec3& BbPosition,
	const glm::vec3& BbSize,
	const glm::vec3& Padding
)
{
	Intersection result{};
	result.Occurred = false;

	// games with gabe
	// https://youtu.be/QxpgtVrjYrg?t=742
	glm::vec3 invDirection = glm::vec3(
		Vector.x != 0.f ? 1.f / Vector.x : FLT_MAX,
		Vector.y != 0.f ? 1.f / Vector.y : FLT_MAX,
		Vector.z != 0.f ? 1.f / Vector.z : FLT_MAX
	);

	glm::vec3 min = BbPosition - BbSize / 2.f - Padding;
	glm::vec3 max = BbPosition + BbSize / 2.f + Padding;

	glm::vec3 t1 = (min - Origin) * invDirection;
	glm::vec3 t2 = (max - Origin) * invDirection;

	glm::vec3 tmin3 = glm::min(t1, t2);
	glm::vec3 tmax3 = glm::max(t1, t2);

	float tmin = std::max({ tmin3.x, tmin3.y, tmin3.z });
	float tmax = std::min({ tmax3.x, tmax3.y, tmax3.z });

	if (tmax < 0.f || tmin > tmax)
		return result;

	float t = (tmin < 0.f) ? tmax : tmin;
	if (t <= 0.f)
		return result;

	result.Occurred = true;
	result.Position = Origin + Vector * t;
	result.Time = t;

	if (tmin == tmin3.x)
		result.Normal.x = (invDirection.x < 0.f) ? 1.f : -1.f;
	else if (tmin == tmin3.y)
		result.Normal.y = (invDirection.y < 0.f) ? 1.f : -1.f;
	else if (tmin == tmin3.z)
		result.Normal.z = (invDirection.z < 0.f) ? 1.f : -1.f;

	return result;
}

IntersectionLib::SweptIntersection IntersectionLib::SweptAabbAabb(
	const glm::vec3& APosition,
	const glm::vec3& ASize,
	const glm::vec3& BPosition,
	const glm::vec3& BSize,
	const glm::vec3& Delta
)
{
	if (glm::length(Delta) == 0.f)
	{
		Intersection hit = AabbAabb(APosition, ASize, BPosition, BSize);

		return {
			BPosition,
			hit,
			hit.Occurred ? 0.f : 1.f
		};
	}

	glm::vec3 bSizeHalf = BSize / 2.f;
	SweptIntersection sweep;

	sweep.Hit = RayAabb(BPosition, Delta, APosition, ASize, bSizeHalf);

	if (sweep.Hit.Occurred)
	{
		sweep.Time = std::clamp(sweep.Hit.Time - EPSILON, 0.f, 1.f);
		sweep.Position = BPosition + (Delta * sweep.Time);

		glm::vec3 direction = glm::normalize(Delta);
		glm::vec3 aSizeHalf = ASize / 2.f;

		sweep.Hit.Position = glm::clamp(
			sweep.Hit.Position + (direction * bSizeHalf),
			APosition - aSizeHalf, APosition + aSizeHalf
		);
	}
	else
	{
		sweep.Position = BPosition + Delta;
		sweep.Time = 1.f;
	}

	return sweep;
}

// https://winter.dev/articles/epa-algorithm

static std::pair<std::vector<glm::vec4>, size_t> getFaceNormals(const std::vector<glm::vec3>& polytope, const std::vector<size_t>& faces)
{
	std::vector<glm::vec4> normals;
	size_t minTriangle = 0;
	float  minDistance = FLT_MAX;

	for (size_t i = 0; i < faces.size(); i += 3)
	{
		glm::vec3 a = polytope[faces[i]];
		glm::vec3 b = polytope[faces[i + 1]];
		glm::vec3 c = polytope[faces[i + 2]];

		glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
		float distance = glm::dot(normal, a);

		if (distance < 0)
		{
			distance *= -1;
			normal *= -1;
		}

		normals.emplace_back(normal, distance);

		if (distance < minDistance)
		{
			minDistance = distance;
			minTriangle = i / 3;
		}
	}

	return { normals, minTriangle };
}

static void addIfUniqueEdge(
	std::vector<std::pair<size_t, size_t>>& edges,
	const std::vector<size_t>& faces,
	size_t a,
	size_t b
)
{
	auto reverse = std::find(                       //      0--<--3
		edges.begin(),                              //     / \ B /   A: 2-0
		edges.end(),                                //    / A \ /    B: 0-2
		std::make_pair(faces[b], faces[a]) //   1-->--2
	);

	if (reverse != edges.end())
		edges.erase(reverse);

	else
		edges.emplace_back(faces[a], faces[b]);
}

static IntersectionLib::CollisionPoints epa(const Gjk::Simplex& Simp, const EcRigidBody* A, const EcRigidBody* B)
{
	ZoneScoped;

	std::vector<glm::vec3> polytope = { Simp.begin(), Simp.end() };
	std::vector<size_t> faces = {
		0, 1, 2,
		0, 3, 1,
		0, 2, 3,
		1, 3, 2
	};

	auto [normals, minFace] = getFaceNormals(polytope, faces);

	glm::vec3 minNormal;
	float minDistance = -FLT_MAX;

	while (minDistance == -FLT_MAX)
	{
		minNormal = glm::vec3(normals[minFace]);
		minDistance = normals[minFace].w;

		glm::vec3 support = Gjk::Support(A, B, minNormal);
		float sDistance = glm::dot(minNormal, support);

		if (abs(sDistance - minDistance) > 0.001f)
		{
			minDistance = FLT_MAX;

			std::vector<std::pair<size_t, size_t>> uniqueEdges;

			for (size_t i = 0; i < normals.size(); i++)
			{
				if (Gjk::SameDirection(normals[i], support))
				{
					size_t f = i * 3;

					addIfUniqueEdge(uniqueEdges, faces, f,     f + 1);
					addIfUniqueEdge(uniqueEdges, faces, f + 1, f + 2);
					addIfUniqueEdge(uniqueEdges, faces, f + 2, f   );

					faces[f + 2] = faces.back(); faces.pop_back();
					faces[f + 1] = faces.back(); faces.pop_back();
					faces[f]     = faces.back(); faces.pop_back();

					normals[i] = normals.back();
					normals.pop_back();

					i--;
				}
			}

			std::vector<size_t> newFaces;
			for (auto [edgeIndex1, edgeIndex2] : uniqueEdges)
			{
				newFaces.push_back(edgeIndex1);
				newFaces.push_back(edgeIndex2);
				newFaces.push_back(polytope.size());
			}

			polytope.push_back(support);

			auto [newNormals, newMinFace] = getFaceNormals(polytope, newFaces);

			float oldMinDistance = FLT_MAX;
			for (size_t i = 0; i < normals.size(); i++)
			{
				if (float dist = normals[i].w; dist < oldMinDistance)
				{
					oldMinDistance = dist;
					minFace = i;
				}
			}

			if (newNormals[newMinFace].w < oldMinDistance)
				minFace = newMinFace + normals.size();

			faces.insert(faces.end(), newFaces.begin(), newFaces.end());
			normals.insert(normals.end(), newNormals.begin(), newNormals.end());
		}
	}

	IntersectionLib::CollisionPoints points;
	points.Normal = minNormal;
	points.PenetrationDepth = minDistance + 0.001f;
	points.HasCollision = true;

	if (points.PenetrationDepth == FLT_MAX)
		points.PenetrationDepth = 0.001f;

	return points;
}

IntersectionLib::CollisionPoints IntersectionLib::Gjk(const EcRigidBody* A, const EcRigidBody* B)
{
	ZoneScoped;

	Gjk::Result result = Gjk::FindIntersection(A, B);

	if (!result.HasIntersection)
		return CollisionPoints{ .HasCollision = false };
	else
		return epa(result.Simp, A, B);
}
