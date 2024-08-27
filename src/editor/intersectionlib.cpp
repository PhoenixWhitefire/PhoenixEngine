#include<unordered_map>

#include"editor/intersectionlib.hpp"

typedef std::unordered_map<IntersectionLib::HittableObject*, std::vector<IntersectionLib::Triangle>> HittableObjectTrianglesCacheType;

static HittableObjectTrianglesCacheType HittableObjectTrianglesCache;

static std::vector<IntersectionLib::Triangle> GetTrianglesFromHittableObject(IntersectionLib::HittableObject* HittableObject)
{
	HittableObjectTrianglesCacheType::iterator it = HittableObjectTrianglesCache.find(HittableObject);

	if (it == HittableObjectTrianglesCache.end())
	{
		Mesh* TargetMesh = HittableObject->CollisionMesh;

		std::vector<IntersectionLib::Triangle> Triangles;
		Triangles.reserve(TargetMesh->Indices.size() / 3);

		for (size_t Indice = 0; Indice * 3 < TargetMesh->Indices.size(); Indice++)
		{
			IntersectionLib::Triangle NewTri = IntersectionLib::Triangle();

			Vector3 Vertex1 = TargetMesh->Vertices[TargetMesh->Indices[Indice * 3]].position;
			Vector3 Vertex2 = TargetMesh->Vertices[TargetMesh->Indices[Indice * 3 + 1]].position;
			Vector3 Vertex3 = TargetMesh->Vertices[TargetMesh->Indices[Indice * 3 + 2]].position;

			Vertex1 = glm::vec3(glm::vec4((glm::vec3)Vertex1, 1.0f) * HittableObject->Matrix);
			Vertex2 = glm::vec3(glm::vec4((glm::vec3)Vertex2, 1.0f) * HittableObject->Matrix);
			Vertex3 = glm::vec3(glm::vec4((glm::vec3)Vertex3, 1.0f) * HittableObject->Matrix);

			Triangles.push_back(NewTri);
		}

		HittableObjectTrianglesCache[HittableObject] = Triangles;

		return Triangles;
	}
	else
	{
		return it->second;
	}
}

const double OneSixth = 1.0f / 6.0f;

static double GetSignedVolume(Vector3 A, Vector3 B, Vector3 C, Vector3 D)
{
	return OneSixth * (((B - A).Cross(C - A)).Dot(D - A));
}

static int GetSign(double Number)
{
	return (0 < Number) - (Number < 0);
}

// Credit: https://stackoverflow.com/a/42752998/16875161
IntersectionLib::IntersectionResult IntersectionLib::Traceline(
	Vector3 LineStart,
	Vector3 LineDirection,
	std::vector<IntersectionLib::HittableObject*> Objects
)
{
	IntersectionResult Result;

	for (HittableObject* Object : Objects)
	{
		Mesh* CurMesh = Object->CollisionMesh;
		auto Tris = GetTrianglesFromHittableObject(Object);

		for (Triangle Tri : Tris)
		{
			Vector3 E1 = Tri.P2 - Tri.P1;
			Vector3 E2 = Tri.P3 - Tri.P1;
			Vector3 N = E1.Cross(E2);

			double det = -LineDirection.Dot(N);
			double invdet = 1.f / det;

			Vector3 AO = LineStart - Tri.P1;
			Vector3 DAO = AO.Cross(LineDirection);

			double u = E2.Dot(DAO) * invdet;
			double v = -E1.Dot(DAO) * invdet;
			double t = AO.Dot(N) * invdet;

			bool DidHit = (det >= 1e-6 && t >= 0.0 && u >= 0.0 && v >= 0.0 && (u + v) <= 1.0);

			if (DidHit)
			{
				Result.DidHit = true;
				Result.HitPosition = LineStart + LineDirection * t;
			}
		}

		if (Result.DidHit)
			break;
	}

	return Result;
}