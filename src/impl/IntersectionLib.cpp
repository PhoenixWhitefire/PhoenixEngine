#include <algorithm>
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

	float scaleX = 1.f / Vector.x;
	float scaleY = 1.f / Vector.y;
	float scaleZ = 1.f / Vector.z;

	int signX = sign(scaleX);
	int signY = sign(scaleY);
	int signZ = sign(scaleZ);

	glm::vec3 bbSizeHalf = BbSize * .5f;

	float nearTimeX = (BbPosition.x - signX * (bbSizeHalf.x + Padding.x) - Origin.x) * scaleX;
	float nearTimeY = (BbPosition.y - signY * (bbSizeHalf.y + Padding.y) - Origin.y) * scaleY;
	float nearTimeZ = (BbPosition.z - signZ * (bbSizeHalf.z + Padding.z) - Origin.z) * scaleZ;

	float farTimeX = (BbPosition.x + signX * (bbSizeHalf.x + Padding.x) - Origin.x) * scaleX;
	float farTimeY = (BbPosition.y + signY * (bbSizeHalf.y + Padding.y) - Origin.y) * scaleY;
	float farTimeZ = (BbPosition.z + signZ * (bbSizeHalf.z + Padding.z) - Origin.z) * scaleZ;

	if (nearTimeX > farTimeX || nearTimeY > farTimeY || nearTimeZ > farTimeZ)
		return result;

	float nearTime = std::max(nearTimeX, std::max(nearTimeY, nearTimeZ));
	float farTime = std::min(farTimeX, std::min(farTimeY, farTimeZ));

	if (nearTime >= 1 || farTime <= 0)
		return result;

	// segment is starting inside the object
	if (nearTime < 0)
		return result;

	result.Occurred = true;

	result.Time = std::clamp(nearTime, 0.f, 1.f);
	result.Position = Origin + (Vector * result.Time);
	result.Depth = glm::length((1.f - result.Time) * Vector);

	if (nearTimeX > nearTimeY)
		if (nearTimeY > nearTimeZ)
			result.Normal = glm::vec3(1.f, 0.f, 0.f);
		else
			result.Normal = glm::vec3(0.f, 0.f, 1.f);
	else
		if (nearTimeY > nearTimeZ)
			result.Normal = glm::vec3(0.f, 1.f, 0.f);
		else
			result.Normal = glm::vec3(0.f, 0.f, 1.f);

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
