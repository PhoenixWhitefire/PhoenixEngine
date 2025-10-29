#include <algorithm>
#include <float.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <tracy/Tracy.hpp>

#include "IntersectionLib.hpp"

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

IntersectionLib::Intersection IntersectionLib::LineAabb(
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

	sweep.Hit = LineAabb(BPosition, Delta, APosition, ASize, bSizeHalf);

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
