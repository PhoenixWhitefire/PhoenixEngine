// G-J-K, 22/01/2026
// https://winter.dev/articles/gjk-algorithm

#include <tracy/Tracy.hpp>
#include <cassert>
#include <array>
#include <cfloat>

#include "geometry/Gjk.hpp"

static const glm::vec3 CubePoints[] = {
    glm::vec3(-0.5f, -0.5f, -0.5f),
    glm::vec3( 0.5f, -0.5f, -0.5f),
    glm::vec3( 0.5f,  0.5f, -0.5f),
    glm::vec3(-0.5f,  0.5f, -0.5f),

    glm::vec3(-0.5f, -0.5f,  0.5f),
    glm::vec3( 0.5f, -0.5f,  0.5f),
    glm::vec3( 0.5f,  0.5f,  0.5f),
    glm::vec3(-0.5f,  0.5f,  0.5f),
};

static glm::vec3 findFurthestPoint(const EcRigidBody* Rb, glm::vec3 Direction)
{
    glm::vec3 maxPoint;
    float maxDistance = -FLT_MAX;

	EcTransform* ct = Rb->CurTransform;
	assert(ct);

    for (const glm::vec3& v : CubePoints)
    {
		glm::vec3 vworld = glm::vec3(ct->Transform * glm::vec4(v * ct->Size, 1.f));
        float distance = glm::dot(vworld, Direction);

        if (distance > maxDistance)
        {
            maxDistance = distance;
            maxPoint = vworld;
        }
    }

    return maxPoint;
}

using namespace Gjk;

glm::vec3 Gjk::Support(const EcRigidBody* A, const EcRigidBody* B, const glm::vec3& Direction)
{
    return findFurthestPoint(A, Direction) - findFurthestPoint(B, -Direction);
}

bool Gjk::SameDirection(const glm::vec3& direction, const glm::vec3& ao)
{
    return glm::dot(direction, ao) > 0;
}

static bool line(Simplex& simp, glm::vec3& direction)
{
    glm::vec3 a = simp[0];
	glm::vec3 b = simp[1];

	glm::vec3 ab = b - a;
	glm::vec3 ao = -a;

	if (SameDirection(ab, ao))
		direction = glm::cross(glm::cross(ab, ao), ab);

	else
    {
		simp = { a };
		direction = ao;
	}

	return false;
}

static bool triangle(Simplex& simp, glm::vec3& direction)
{
    glm::vec3 a = simp[0];
	glm::vec3 b = simp[1];
	glm::vec3 c = simp[2];

	glm::vec3 ab = b - a;
	glm::vec3 ac = c - a;
	glm::vec3 ao =   - a;

	glm::vec3 abc = glm::cross(ab, ac);

	if (SameDirection(glm::cross(abc, ac), ao))
    {
		if (SameDirection(ac, ao))
        {
			simp = { a, c };
			direction = glm::cross(glm::cross(ac, ao), ac);
		}

		else
        {
			return line(simp = { a, b }, direction);
		}
	}

	else
    {
		if (SameDirection(glm::cross(ab, abc), ao))
			return line(simp = { a, b }, direction);

		else
        {
			if (SameDirection(abc, ao))
				direction = abc;

			else
            {
				simp = { a, c, b };
				direction = -abc;
			}
		}
	}

	return false;
}

static bool tetrahedron(Simplex& simp, glm::vec3& direction)
{
    glm::vec3 a = simp[0];
	glm::vec3 b = simp[1];
	glm::vec3 c = simp[2];
	glm::vec3 d = simp[3];

	glm::vec3 ab = b - a;
	glm::vec3 ac = c - a;
	glm::vec3 ad = d - a;
	glm::vec3 ao =   - a;

	glm::vec3 abc = glm::cross(ab, ac);
	glm::vec3 acd = glm::cross(ac, ad);
	glm::vec3 adb = glm::cross(ad, ab);

	if (SameDirection(abc, ao))
		return triangle(simp = { a, b, c }, direction);

	if (SameDirection(acd, ao))
		return triangle(simp = { a, c, d }, direction);

	if (SameDirection(adb, ao))
		return triangle(simp = { a, d, b }, direction);

	return true;
}

static bool nextSimplex(Simplex& simp, glm::vec3& direction)
{
    switch (simp.size())
    {
    case 2:
        return line(simp, direction);
    case 3:
        return triangle(simp, direction);
    case 4:
        return tetrahedron(simp, direction);
    [[unlikely]] default: assert(false);
    }

    assert(false);
    return false;
}

Result Gjk::FindIntersection(const EcRigidBody* A, const EcRigidBody* B)
{
	ZoneScoped;

    glm::vec3 s = Support(A, B, glm::vec3(1.f, 0.f, 0.f));

	Result result;
    result.Simp.push_front(s);

    glm::vec3 direction = -s;

    while (true)
    {
        s = Support(A, B, direction);

        if (glm::dot(s, direction) <= 0)
		{
			result.HasIntersection = false;
            return result; // no intersection
		}

        result.Simp.push_front(s);

        if (nextSimplex(result.Simp, direction))
		{
			result.HasIntersection = true;
            return result; // intersection
		}
    }
}
