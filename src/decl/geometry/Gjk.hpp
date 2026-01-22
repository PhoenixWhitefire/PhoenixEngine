// G-J-K, 22/01/2026

#pragma once

#include <glm/vec3.hpp>

#include "component/RigidBody.hpp"

namespace Gjk
{
    struct Cube
    {
        glm::vec3 Position;
        glm::vec3 Size;
    };

    struct Simplex
    {
	    Simplex()
			: Size(0)
	    {
		}

	    Simplex& operator=(std::initializer_list<glm::vec3> list)
	    {
			Size = 0;

			for (glm::vec3 point : list)
				Points[Size++] = point;

			return *this;
	    }

	    void push_front(glm::vec3 point)
	    {
			Points = { point, Points[0], Points[1], Points[2] };
			Size = std::min(Size + 1, 4);
	    }

	    glm::vec3& operator[](int i) { return Points[i]; }
	    size_t size() const { return Size; }

	    auto begin() const { return Points.begin(); }
	    auto end() const { return Points.end() - (4 - Size); }

	    std::array<glm::vec3, 4> Points;
	    int Size;
    };

    struct Result
    {
        Simplex Simp;
        bool HasIntersection = false;
    };

    glm::vec3 Support(const EcRigidBody* A, const EcRigidBody* B, const glm::vec3& Direction);
    bool SameDirection(const glm::vec3& A, const glm::vec3& B);
    Result FindIntersection(const EcRigidBody* A, const EcRigidBody* B);
};
