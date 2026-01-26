// G-J-K, 22/01/2026

#pragma once

#include <glm/vec3.hpp>

#include "component/RigidBody.hpp"

namespace Gjk
{
    struct SupportPoint
	{
		glm::vec3 P;  // Minkowski (A - B)
		glm::vec3 A;  // Point on A
		glm::vec3 B;  // Point on B
	};

    struct Simplex
    {
	    Simplex()
			: Size(0)
	    {
		}

	    Simplex& operator=(std::initializer_list<SupportPoint> list)
	    {
			Size = 0;

			for (const SupportPoint& point : list)
				Points[Size++] = point;

			return *this;
	    }

	    void push_front(const SupportPoint& point)
	    {
			Points = { point, Points[0], Points[1], Points[2] };
			Size = std::min(Size + 1, 4);
	    }

	    SupportPoint& operator[](int i) { return Points[i]; }
	    size_t size() const { return Size; }

	    auto begin() const { return Points.begin(); }
	    auto end() const { return Points.end() - (4 - Size); }

	    std::array<SupportPoint, 4> Points = {};
	    int Size;
    };

    struct Result
    {
        Simplex Simp;
        bool HasIntersection = false;
    };

    SupportPoint Support(const EcRigidBody* A, const EcRigidBody* B, const glm::vec3& Direction);
    bool SameDirection(const glm::vec3& A, const glm::vec3& B);
    Result FindIntersection(const EcRigidBody* A, const EcRigidBody* B);
};
