#include<editor/intersectionlib.hpp>

typedef std::unordered_map<IntersectionLib::HittableObject*, std::vector<IntersectionLib::Triangle>> HittableObjectTrianglesCacheType;

static HittableObjectTrianglesCacheType HittableObjectTrianglesCache;

std::vector<IntersectionLib::Triangle> GetTrianglesFromHittableObject(IntersectionLib::HittableObject* HittableObject)
{
	HittableObjectTrianglesCacheType::iterator it = HittableObjectTrianglesCache.find(HittableObject);

	if (it == HittableObjectTrianglesCache.end())
	{
		Mesh* TargetMesh = HittableObject->CollisionMesh;

		std::vector<IntersectionLib::Triangle> Triangles;
		Triangles.reserve(TargetMesh->Indices.size() / 3);

		for (unsigned int Indice = 0; Indice * 3 < TargetMesh->Indices.size(); Indice++)
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

double GetSignedVolume(Vector3 A, Vector3 B, Vector3 C, Vector3 D)
{
	return OneSixth * ((B - A).Cross(C - A).Dot(D - A));
}

int GetSign(double Number)
{
	return (0 < Number) - (Number < 0);
}

IntersectionLib::IntersectionResult IntersectionLib::Traceline(Vector3 LineStart, Vector3 LineEnd, std::vector<IntersectionLib::HittableObject*> Objects)
{
	IntersectionResult Result;

	for (HittableObject* Object : Objects)
	{
		Mesh* CurMesh = Object->CollisionMesh;
		auto Tris = GetTrianglesFromHittableObject(Object);

		for (Triangle Tri : Tris)
		{
			int Sign1 = GetSign(GetSignedVolume(LineStart, Tri.P1, Tri.P2, Tri.P3));
			int Sign2 = (GetSignedVolume(LineEnd, Tri.P1, Tri.P2, Tri.P3));

			if (Sign1 != Sign2)
			{
				int Sign3 = (GetSignedVolume(LineStart, LineEnd, Tri.P1, Tri.P2));
				int Sign4 = (GetSignedVolume(LineStart, LineEnd, Tri.P2, Tri.P3));
				int Sign5 = (GetSignedVolume(LineStart, LineEnd, Tri.P3, Tri.P1));

				if (Sign3 == Sign4 && Sign4 == Sign5)
				{
					// there is an intersection

					Result.DidHit = true;
					Result.HitObjectId = Object->Id;

					Vector3 N = (Tri.P2 - Tri.P1).Cross(Tri.P3 - Tri.P1);
					double T = -(LineStart - Tri.P1).Dot(N) / (LineStart - LineEnd).Dot(N);

					Result.HitPosition = LineStart + ((LineEnd - LineStart) * T);

					break;
				}
			}
		}

		if (Result.DidHit)
			break;
	}

	return Result;
}