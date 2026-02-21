// G-J-K, 22/01/2026
// https://winter.dev/articles/gjk-algorithm

#include <tracy/Tracy.hpp>
#include <cassert>
#include <array>
#include <cfloat>

#include "geometry/Gjk.hpp"
#include "component/Mesh.hpp"
#include "asset/MeshProvider.hpp"

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

static glm::vec3 findFurthestPoint_Mesh(const EcRigidBody* Rb, glm::vec3 Direction, const Mesh& mesh, float* maxDistance, glm::vec3 maxPoint, const glm::mat4& submeshTrans = glm::mat4(1.f))
{
	assert(mesh.MeshDataPreserved);

	EcTransform* ct = Rb->CurTransform;
	assert(ct);

	for (uint32_t ind : mesh.Indices)
	{
		const Vertex& v = mesh.Vertices[ind];

		glm::vec3 vworld = glm::vec3(ct->Transform * submeshTrans * glm::vec4(v.Position * ct->Size, 1.f));
        float distance = glm::dot(vworld, Direction);

        if (distance > *maxDistance)
        {
            *maxDistance = distance;
            maxPoint = vworld;
        }
    }

    return maxPoint;
}

static glm::vec3 findFurthestPoint_MeshComponent(const EcRigidBody* Rb, glm::vec3 Direction)
{
	EcMesh* cm = Rb->Object->FindComponent<EcMesh>();

	uint32_t meshId = cm ? cm->RenderMeshId : 0;
	const Mesh& mesh = MeshProvider::Get()->GetMeshResource(meshId);

	float maxDistance = -FLT_MAX;
	return findFurthestPoint_Mesh(Rb, Direction, mesh, &maxDistance, glm::vec3(0.f));
}

static glm::vec3 findFurthestPoint_Cube(const EcRigidBody* Rb, glm::vec3 Direction)
{
	glm::mat3 rotation = glm::mat3(Rb->CurTransform->Transform);

	glm::vec3 localDir = glm::transpose(rotation) * Direction;
	glm::vec3 result;

	glm::vec3 halfSize = Rb->CurTransform->Size / 2.f;

	result.x = (localDir.x > 0) ? halfSize.x : -halfSize.x;
	result.y = (localDir.y > 0) ? halfSize.y : -halfSize.y;
	result.z = (localDir.z > 0) ? halfSize.z : -halfSize.z;

	return rotation * result + glm::vec3(Rb->CurTransform->Transform[3]);
}

static glm::vec3 findFurthestPoint_Sphere(const EcRigidBody* Rb, glm::vec3 Direction)
{
	glm::vec3 center = glm::vec3(Rb->CurTransform->Transform[3]);

	if (glm::length(Direction) < 0.0001f)
		return center;
	return center + glm::normalize(Direction) * (Rb->CurTransform->Size.x / 2.f);
}

static glm::vec3 findFurthestPoint_Hulls(const EcRigidBody* Rb, glm::vec3 Direction)
{
	MeshProvider* meshProv = MeshProvider::Get();
	float maxDistance = -FLT_MAX;
	glm::vec3 maxPoint;

	for (const EcRigidBody::Hull& hull : Rb->Hulls)
	{
		const Mesh& mesh = meshProv->GetMeshResource(hull.MeshId);
		maxPoint = findFurthestPoint_Mesh(Rb, Direction, mesh, &maxDistance, maxPoint, hull.Transform);
	}

	return Rb->Hulls.size() > 0 ? maxPoint : glm::vec3();
}

static glm::vec3 findFurthestPoint(const EcRigidBody* Rb, glm::vec3 Direction)
{
	switch (Rb->CollisionType)
	{
	case EnCollisionType::Sphere:
		return findFurthestPoint_Sphere(Rb, Direction);

	case EnCollisionType::Hulls:
		return findFurthestPoint_Hulls(Rb, Direction);

	case EnCollisionType::MeshComponent:
		return  findFurthestPoint_MeshComponent(Rb, Direction);

	case EnCollisionType::Cube: default:
		return findFurthestPoint_Cube(Rb, Direction);
	}
}

using namespace Gjk;

SupportPoint Gjk::Support(const EcRigidBody* A, const EcRigidBody* B, const glm::vec3& Direction)
{
	glm::vec3 pA = findFurthestPoint(A, Direction);
	glm::vec3 pB = findFurthestPoint(B, -Direction);

    return SupportPoint{ .P = pA - pB, .A = pA, .B = pB };
}

bool Gjk::SameDirection(const glm::vec3& direction, const glm::vec3& ao)
{
    return glm::dot(direction, ao) > 0;
}

static bool line(Simplex& simp, glm::vec3& direction)
{
    const SupportPoint& a = simp[0];
	const SupportPoint& b = simp[1];

	glm::vec3 ab = b.P - a.P;
	glm::vec3 ao = -a.P;

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
    const SupportPoint& a = simp[0];
	const SupportPoint& b = simp[1];
	const SupportPoint& c = simp[2];

	glm::vec3 ab = b.P - a.P;
	glm::vec3 ac = c.P - a.P;
	glm::vec3 ao =  -a.P;

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
    const SupportPoint& a = simp[0];
	const SupportPoint& b = simp[1];
	const SupportPoint& c = simp[2];
	const SupportPoint& d = simp[3];

	glm::vec3 ab = b.P - a.P;
	glm::vec3 ac = c.P - a.P;
	glm::vec3 ad = d.P - a.P;
	glm::vec3 ao = -a.P;

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

    SupportPoint s = Support(A, B, glm::vec3(1.f, 0.f, 0.f));

	Result result;
    result.Simp.push_front(s);

    glm::vec3 direction = -s.P;

	size_t numIterations = 0;

    while (true)
    {
        s = Support(A, B, direction);

        if (glm::dot(s.P, direction) <= 0)
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

		numIterations++;

		if (numIterations > 1e5)
		{
			result.HasIntersection = false;
			Log::Warning("Too many iterations!", "Physics Gjk::FindIntersection");

			return result;
		}
    }
}
