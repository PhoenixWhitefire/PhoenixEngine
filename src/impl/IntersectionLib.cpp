#include "IntersectionLib.hpp"
#include "Profiler.hpp"

template<class T> static int sign(T v)
{
	return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

IntersectionLib::Intersection IntersectionLib::AabbAabb
(
	glm::vec3& APosition,
	glm::vec3& ASize,
	glm::vec3& BPosition,
	glm::vec3& BSize
)
{
	PROFILER_PROFILE_SCOPE("Intersection::AabbAabb");

	Intersection result{};
	result.Occurred = false;

	glm::vec3 aSizeHalf = ASize * .5f;
	glm::vec3 bSizeHalf = BSize * .5f;

	// https://noonat.github.io/intersect/
	double dx = BPosition.x - APosition.x;
	double px = (bSizeHalf.x + aSizeHalf.x) - std::abs(dx);
	if (px <= 0.f)
		return result;

	double dy = BPosition.y - APosition.y;
	double py = (bSizeHalf.y + aSizeHalf.y) - std::abs(dy);
	if (py <= 0.f)
		return result;

	double dz = BPosition.z - APosition.z;
	double pz = (bSizeHalf.z + aSizeHalf.z) - std::abs(dz);
	if (pz <= 0.f)
		return result;

	result.Occurred = true;

	if (px < py && px < pz)
	{
		int sx = sign(dx);
		result.Depth = px * sx;
		result.Normal = Vector3(-sx, 0.f, 0.f);
		result.Vector = Vector3(APosition.x + (aSizeHalf.x * sx), 0.f, 0.f);
	}
	else if (py < px && py < pz)
	{
		int sy = sign(dy);
		result.Depth = py * sy;
		result.Normal = Vector3(0.f, -sy, 0.f);
		result.Vector = Vector3(0.f, APosition.y + (aSizeHalf.y * sy), 0.f);
	}
	else if (pz < py)
	{
		int sz = sign(dz);
		result.Depth = pz * sz;
		result.Normal = Vector3(0.f, 0.f, -sz);
		result.Vector = Vector3(0.f, 0.f, APosition.z + (aSizeHalf.z * sz));
	}

	return result;
}

IntersectionLib::Intersection IntersectionLib::LineAabb(
	glm::vec3& Origin,
	glm::vec3& Vector,
	glm::vec3& BbPosition,
	glm::vec3& BbSize,
	float PaddingX,
	float PaddingY,
	float PaddingZ
)
{
	PROFILER_PROFILE_SCOPE("Intersection::LineAabb");

	Intersection result{};
	result.Occurred = false;

	float scaleX = 1.f / Vector.x;
	float scaleY = 1.f / Vector.y;
	float scaleZ = 1.f / Vector.z;

	int signX = sign(scaleX);
	int signY = sign(scaleY);
	int signZ = sign(scaleZ);

	glm::vec3 bbSizeHalf = BbSize * .5f;

	float nearTimeX = (BbPosition.x - signX * (bbSizeHalf.x + PaddingX) - Origin.x) * scaleX;
	float nearTimeY = (BbPosition.y - signY * (bbSizeHalf.y + PaddingY) - Origin.y) * scaleY;
	float nearTimeZ = (BbPosition.z - signZ * (bbSizeHalf.z + PaddingZ) - Origin.z) * scaleZ;

	float farTimeX = (BbPosition.x + signX * (bbSizeHalf.x + PaddingX) - Origin.x) * scaleX;
	float farTimeY = (BbPosition.y + signY * (bbSizeHalf.y + PaddingY) - Origin.y) * scaleY;
	float farTimeZ = (BbPosition.z + signZ * (bbSizeHalf.z + PaddingZ) - Origin.z) * scaleZ;

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

	float time = std::clamp(nearTime, 0.f, 1.f);

	result.Vector = Origin + Vector * time;
	result.Depth = glm::length((1.f - time) * Vector);

	if (nearTimeX > nearTimeY)
		if (nearTimeY > nearTimeZ)
			result.Normal = Vector3::xAxis;
		else
			result.Normal = Vector3::zAxis;
	else
		if (nearTimeY > nearTimeZ)
			result.Normal = Vector3::yAxis;
		else
			result.Normal = Vector3::zAxis;

	return result;
}


