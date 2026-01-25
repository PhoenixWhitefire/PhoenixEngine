#define _USE_MATH_DEFINES
#include <math.h>
#include <map>

#include "asset/PrimitiveMeshes.hpp"

constexpr glm::vec3 VecXAxis = { 1.f, 0.f, 0.f };
constexpr glm::vec3 VecYAxis = { 0.f, 1.f, 0.f };
constexpr glm::vec3 VecZAxis = { 0.f, 0.f, 1.f };

// white vertex color
constexpr glm::vec4 White = { 1.f, 1.f, 1.f, 1.f };

Mesh PrimitiveMeshes::Cube()
{
	std::vector<Vertex> vertices = {
		// front face
		Vertex{ glm::vec3(-.5f,  .5f, -.5f), -VecZAxis, White, glm::vec2(1.f, 0.f) },
		Vertex{ glm::vec3(-.5f, -.5f, -.5f), -VecZAxis, White, glm::vec2(1.f, 1.f) },
		Vertex{ glm::vec3( .5f, -.5f, -.5f), -VecZAxis, White, glm::vec2(0.f, 1.f) },
		Vertex{ glm::vec3( .5f,  .5f, -.5f), -VecZAxis, White, glm::vec2(0.f, 0.f) },

		// back face
		Vertex{ glm::vec3(-.5f,  .5f,  .5f),  VecZAxis, White, glm::vec2(0.f, 0.f) },
		Vertex{ glm::vec3(-.5f, -.5f,  .5f),  VecZAxis, White, glm::vec2(0.f, 1.f) },
		Vertex{ glm::vec3( .5f, -.5f,  .5f),  VecZAxis, White, glm::vec2(1.f, 1.f) },
		Vertex{ glm::vec3( .5f,  .5f,  .5f),  VecZAxis, White, glm::vec2(1.f, 0.f) },

		// right face
		// it is at this point that i am wondering if i can just read a .obj cube from blender or smthing
		// instead of doing this by hand and by struggling to imaging where the vertices are in my head
		Vertex{ glm::vec3( .5f,  .5f, -.5f), VecXAxis, White, glm::vec2(1.f, 0.f) },
		Vertex{ glm::vec3( .5f, -.5f, -.5f), VecXAxis, White, glm::vec2(1.f, 1.f) },
		Vertex{ glm::vec3( .5f, -.5f,  .5f), VecXAxis, White, glm::vec2(0.f, 1.f) },
		Vertex{ glm::vec3( .5f,  .5f,  .5f), VecXAxis, White, glm::vec2(0.f, 0.f) },

		// left face
		Vertex{ glm::vec3(-.5f,  .5f, -.5f), -VecXAxis, White, glm::vec2(0.f, 0.f) },
		Vertex{ glm::vec3(-.5f, -.5f, -.5f), -VecXAxis, White, glm::vec2(0.f, 1.f) },
		Vertex{ glm::vec3(-.5f, -.5f,  .5f), -VecXAxis, White, glm::vec2(1.f, 1.f) },
		Vertex{ glm::vec3(-.5f,  .5f,  .5f), -VecXAxis, White, glm::vec2(1.f, 0.f) },

		// bottom face
		Vertex{ glm::vec3( .5f, -.5f, -.5f), -VecYAxis, White, glm::vec2(0.f, 0.f) },
		Vertex{ glm::vec3(-.5f, -.5f, -.5f), -VecYAxis, White, glm::vec2(1.f, 0.f) },
		Vertex{ glm::vec3(-.5f, -.5f,  .5f), -VecYAxis, White, glm::vec2(1.f, 1.f) },
		Vertex{ glm::vec3( .5f, -.5f,  .5f), -VecYAxis, White, glm::vec2(0.f, 1.f) },

		// top face
		Vertex{ glm::vec3( .5f,  .5f, -.5f),  VecYAxis, White, glm::vec2(0.f, 1.f) },
		Vertex{ glm::vec3(-.5f,  .5f, -.5f),  VecYAxis, White, glm::vec2(1.f, 1.f) },
		Vertex{ glm::vec3(-.5f,  .5f,  .5f),  VecYAxis, White, glm::vec2(1.f, 0.f) },
		Vertex{ glm::vec3( .5f,  .5f,  .5f),  VecYAxis, White, glm::vec2(0.f, 0.f) }
	};

	// counter-clockwise order
	std::vector<uint32_t> indices = {
		// front face
		0, 1, 2,
		2, 3, 0,

		4, 7, 6,
		6, 5, 4,

		// right face
		8, 9, 10,
		10, 11, 8,

		// left face
		12, 15, 14,
		14, 13, 12,

		// bottom face
		16, 17, 18,
		18, 19, 16,

		// top face
		20, 23, 22,
		22, 21, 20
	};

	return Mesh{ vertices, indices, {}, UINT32_MAX, true };
}

Mesh PrimitiveMeshes::Quad()
{
	std::vector<Vertex> vertices = {
		Vertex{ glm::vec3(  .5f, -.5f,  0.f ), -VecZAxis, White, glm::vec2(1.f, 1.f) },
		Vertex{ glm::vec3( -.5f, -.5f,  0.f ), -VecZAxis, White, glm::vec2(0.f, 1.f) },
		Vertex{ glm::vec3( -.5f,  .5f,  0.f ), -VecZAxis, White, glm::vec2(0.f, 0.f) },
		Vertex{ glm::vec3(  .5f,  .5f,  0.f ), -VecZAxis, White, glm::vec2(1.f, 0.f) }
	};

	std::vector<uint32_t> indices = {
		0, 1, 2,
		3, 0, 2
	};

	return Mesh{ vertices, indices, {}, UINT32_MAX, true };
}

Mesh PrimitiveMeshes::Sphere()
{
	// https://stackoverflow.com/a/47416720
	constexpr int HorizontalLines = 15;
	constexpr int VerticalLines = 15;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	for (int y = 0; y <= HorizontalLines; y++)
	{
		float v = (float)y / HorizontalLines;
		float phi = v * M_PI;

		for (int x = 0; x <= VerticalLines; x++)
		{
			float u = (float)x / VerticalLines;
			float theta = u * 2.f * M_PI;

			glm::vec3 pos = glm::normalize(glm::vec3(
			    sin(phi) * cos(theta),
			    cos(phi),
			    sin(phi) * sin(theta)
			));

			vertices.emplace_back(
			    pos * 0.5f,
			    pos,
			    White,
				glm::vec2(u, 1.f - v)
			);
		}
	}

	for (int y = 0; y < HorizontalLines; y++)
	{
	    for (int x = 0; x < VerticalLines; x++)
	    {
	        uint32_t i0 = y * (VerticalLines + 1) + x;
	        uint32_t i1 = i0 + 1;
	        uint32_t i2 = (y + 1) * (VerticalLines + 1) + x;
	        uint32_t i3 = i2 + 1;

	        indices.push_back(i0);
	        indices.push_back(i2);
	        indices.push_back(i1);

	        indices.push_back(i1);
	        indices.push_back(i2);
	        indices.push_back(i3);
	    }
	}

	return Mesh{ vertices, indices, {}, UINT32_MAX, true };
}
