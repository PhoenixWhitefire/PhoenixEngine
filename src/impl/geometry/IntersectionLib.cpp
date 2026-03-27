#include <algorithm>
#include <cfloat>
#include <utility>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <tracy/Tracy.hpp>

#include "geometry/IntersectionLib.hpp"
#include "geometry/Gjk.hpp"

static constexpr float EPSILON = 1e-8f;
static constexpr float EPA_EPSILON = 0.001f;
static constexpr float EPA_MAX_ITER = 64.f;

template<class T> static int sign(T v)
{
	return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

struct EpaFace
{
	size_t A;
	size_t B;
	size_t C;
	glm::vec3 Normal;
	float Distance;
};

static EpaFace makeFace(const std::vector<Gjk::SupportPoint>& polytope, size_t a, size_t b, size_t c)
{
	const glm::vec3& pa = polytope[a].P;
	const glm::vec3& pb = polytope[b].P;
	const glm::vec3& pc = polytope[c].P;

	glm::vec3 normal = glm::cross(pb - pa, pc - pa);
	float len2 = glm::dot(normal, normal);

	if (len2 <= 1e-20f)
		return { a, b, c, glm::vec3(0.f), FLT_MAX };

	normal /= std::sqrt(len2);
	float distance = glm::dot(normal, pa);

	if (distance < 0.f)
	{
		normal = -normal;
		distance = -distance;
	}

	return { a, b, c, normal, distance };
}

static void addIfUniqueEdge(
	std::vector<std::pair<size_t, size_t>>& edges,
	size_t a,
	size_t b
)
{
	auto reverse = std::find(edges.begin(), edges.end(), std::make_pair(b, a));

	if (reverse != edges.end())
		edges.erase(reverse);
	else
		edges.emplace_back(a, b);
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

	glm::vec3 aHalf = ASize * 0.5f;
	glm::vec3 bHalf = BSize * 0.5f;

	glm::vec3 delta = BPosition - APosition;
	glm::vec3 overlap = (aHalf + bHalf) - glm::abs(delta);

	if (overlap.x <= 0.f || overlap.y <= 0.f || overlap.z <= 0.f)
		return result;

	result.Occurred = true;

	if (overlap.x < overlap.y && overlap.x < overlap.z)
	{
		int sx = sign(delta.x);
		result.Depth = overlap.x * sx;
		result.Normal = glm::vec3(-sx, 0.f, 0.f);
		result.Position = glm::vec3(APosition.x + aHalf.x * sx, BPosition.y, BPosition.z);
	}
	else if (overlap.y < overlap.z)
	{
		int sy = sign(delta.y);
		result.Depth = overlap.y * sy;
		result.Normal = glm::vec3(0.f, -sy, 0.f);
		result.Position = glm::vec3(BPosition.x, APosition.y + aHalf.y * sy, BPosition.z);
	}
	else
	{
		int sz = sign(delta.z);
		result.Depth = overlap.z * sz;
		result.Normal = glm::vec3(0.f, 0.f, -sz);
		result.Position = glm::vec3(BPosition.x, BPosition.y, APosition.z + aHalf.z * sz);
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

	glm::vec3 invDirection(
		Vector.x != 0.f ? 1.f / Vector.x : FLT_MAX,
		Vector.y != 0.f ? 1.f / Vector.y : FLT_MAX,
		Vector.z != 0.f ? 1.f / Vector.z : FLT_MAX
	);

	glm::vec3 half = BbSize * 0.5f;
	glm::vec3 min = BbPosition - half - Padding;
	glm::vec3 max = BbPosition + half + Padding;

	glm::vec3 t1 = (min - Origin) * invDirection;
	glm::vec3 t2 = (max - Origin) * invDirection;

	glm::vec3 tmin3 = glm::min(t1, t2);
	glm::vec3 tmax3 = glm::max(t1, t2);

	float tmin = std::max(tmin3.x, std::max(tmin3.y, tmin3.z));
	float tmax = std::min(tmax3.x, std::min(tmax3.y, tmax3.z));

	if (tmax < 0.f || tmin > tmax)
		return result;

	float t = tmin > 0.f ? tmin : tmax;
	if (t <= 0.f)
		return result;

	result.Occurred = true;
	result.Position = Origin + Vector * t;
	result.Time = t;

	if (tmin == tmin3.x)
		result.Normal.x = invDirection.x < 0.f ? 1.f : -1.f;
	else if (tmin == tmin3.y)
		result.Normal.y = invDirection.y < 0.f ? 1.f : -1.f;
	else
		result.Normal.z = invDirection.z < 0.f ? 1.f : -1.f;

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
	SweptIntersection sweep{};

	if (Delta.x == 0.f && Delta.y == 0.f && Delta.z == 0.f)
	{
		Intersection hit = AabbAabb(APosition, ASize, BPosition, BSize);

		sweep.Position = BPosition;
		sweep.Hit = hit;
		sweep.Time = hit.Occurred ? 0.f : 1.f;
		return sweep;
	}

	glm::vec3 bHalf = BSize * 0.5f;
	sweep.Hit = RayAabb(BPosition, Delta, APosition, ASize, bHalf);

	if (sweep.Hit.Occurred)
	{
		sweep.Time = std::clamp(sweep.Hit.Time - EPSILON, 0.f, 1.f);
		sweep.Position = BPosition + Delta * sweep.Time;

		glm::vec3 aHalf = ASize * 0.5f;
		glm::vec3 direction = glm::normalize(Delta);

		sweep.Hit.Position = glm::clamp(
			sweep.Hit.Position + direction * bHalf,
			APosition - aHalf,
			APosition + aHalf
		);
	}
	else
	{
		sweep.Position = BPosition + Delta;
		sweep.Time = 1.f;
	}

	return sweep;
}

static std::pair<std::vector<EpaFace>, size_t> buildFaces(const std::vector<Gjk::SupportPoint>& polytope, const std::vector<size_t>& indices)
{
	std::vector<EpaFace> faces;
	faces.reserve(indices.size() / 3);

	size_t minFace = 0;
	float minDistance = FLT_MAX;

	for (size_t i = 0; i < indices.size(); i += 3)
	{
		EpaFace face = makeFace(polytope, indices[i], indices[i + 1], indices[i + 2]);
		faces.push_back(face);

		if (face.Distance < minDistance)
		{
			minDistance = face.Distance;
			minFace = faces.size() - 1;
		}
	}

	return { std::move(faces), minFace };
}

static IntersectionLib::CollisionPoints epa(const Gjk::Simplex& Simp, const EcRigidBody* A, const EcRigidBody* B)
{
	ZoneScoped;

	std::vector<Gjk::SupportPoint> polytope(Simp.begin(), Simp.end());
	if (polytope.size() < 4)
		return IntersectionLib::CollisionPoints{ .HasCollision = false };

	std::vector<size_t> indices = {
		0, 1, 2,
		0, 3, 1,
		0, 2, 3,
		1, 3, 2
	};

	auto [faces, minFace] = buildFaces(polytope, indices);

	for (size_t iter = 0; iter < 64; ++iter)
	{
		if (faces.empty())
			break;

		const EpaFace& face = faces[minFace];
		glm::vec3 normal = face.Normal;
		float distance = face.Distance;

		Gjk::SupportPoint support = Gjk::Support(A, B, normal);
		float sDistance = glm::dot(normal, support.P);

		if (sDistance - distance <= EPA_EPSILON)
		{
			const Gjk::SupportPoint& a = polytope[face.A];
			const Gjk::SupportPoint& b = polytope[face.B];
			const Gjk::SupportPoint& c = polytope[face.C];

			glm::vec3 v0 = b.P - a.P;
			glm::vec3 v1 = c.P - a.P;
			glm::vec3 v2 = -a.P;

			float d00 = glm::dot(v0, v0);
			float d01 = glm::dot(v0, v1);
			float d11 = glm::dot(v1, v1);
			float d20 = glm::dot(v2, v0);
			float d21 = glm::dot(v2, v1);
			float denom = d00 * d11 - d01 * d01;

			if (std::abs(denom) <= EPSILON)
				return IntersectionLib::CollisionPoints{ .HasCollision = false };

			float v = (d11 * d20 - d01 * d21) / denom;
			float w = (d00 * d21 - d01 * d20) / denom;
			float u = 1.f - v - w;

			IntersectionLib::CollisionPoints points;
			points.A = (u * a.A) + (v * b.A) + (w * c.A);
			points.B = (u * a.B) + (v * b.B) + (w * c.B);
			points.Normal = normal;
			points.PenetrationDepth = distance;
			points.HasCollision = true;

			if (points.PenetrationDepth <= 0.f)
				points.PenetrationDepth = EPA_EPSILON;

			return points;
		}

		std::vector<std::pair<size_t, size_t>> edges;
		edges.reserve(faces.size() * 3);

		for (size_t i = 0; i < faces.size();)
		{
			const EpaFace& f = faces[i];
			if (glm::dot(f.Normal, support.P) > glm::dot(f.Normal, polytope[f.A].P))
			{
				addIfUniqueEdge(edges, f.A, f.B);
				addIfUniqueEdge(edges, f.B, f.C);
				addIfUniqueEdge(edges, f.C, f.A);

				faces[i] = faces.back();
				faces.pop_back();
				if (minFace == faces.size())
					minFace = i;
				continue;
			}
			++i;
		}

		size_t newIndex = polytope.size();
		polytope.push_back(support);

		for (auto [a, b] : edges)
			faces.push_back(makeFace(polytope, a, b, newIndex));

		minFace = 0;
		float minDistance = FLT_MAX;
		for (size_t i = 0; i < faces.size(); ++i)
		{
			if (faces[i].Distance < minDistance)
			{
				minDistance = faces[i].Distance;
				minFace = i;
			}
		}
	}

	return IntersectionLib::CollisionPoints{ .HasCollision = false };
}

IntersectionLib::CollisionPoints IntersectionLib::Gjk(const EcRigidBody* A, const EcRigidBody* B)
{
	ZoneScoped;

	Gjk::Result result = Gjk::FindIntersection(A, B);

	if (!result.HasIntersection)
		return CollisionPoints{ .HasCollision = false };

	return epa(result.Simp, A, B);
}
