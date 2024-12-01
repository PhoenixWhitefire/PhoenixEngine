#include "asset/PrimitiveMeshes.hpp"
#include "datatype/Vector3.hpp"

Mesh PrimitiveMeshes::Cube()
{
	// white vertex color
	glm::vec3 white(1.0f, 1.0f, 1.0f);

	std::vector<Vertex> vertices =
	{
		// front face
		Vertex{ glm::vec3(-0.5f,  0.5f, -0.5f), -Vector3::zAxis, white, glm::vec2(1.0f, 0.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f, -0.5f), -Vector3::zAxis, white, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f, -0.5f, -0.5f), -Vector3::zAxis, white, glm::vec2(0.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f,  0.5f, -0.5f), -Vector3::zAxis, white, glm::vec2(0.0f, 0.0f) },

		// back face
		Vertex{ glm::vec3(-0.5f,  0.5f,  0.5f),  Vector3::zAxis, white, glm::vec2(0.0f, 0.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f,  0.5f),  Vector3::zAxis, white, glm::vec2(0.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f, -0.5f,  0.5f),  Vector3::zAxis, white, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f,  0.5f,  0.5f),  Vector3::zAxis, white, glm::vec2(1.0f, 0.0f) },

		// right face
		// it is at this point that i am wondering if i can just read a .obj cube from blender or smthing
		// instead of doing this by hand and by struggling to imaging where the vertices are in my head
		Vertex{ glm::vec3( 0.5f,  0.5f, -0.5f), -Vector3::xAxis, white, glm::vec2(1.0f, 0.0f) },
		Vertex{ glm::vec3( 0.5f, -0.5f, -0.5f), -Vector3::xAxis, white, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f, -0.5f,  0.5f), -Vector3::xAxis, white, glm::vec2(0.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f,  0.5f,  0.5f), -Vector3::xAxis, white, glm::vec2(0.0f, 0.0f) },

		// left face
		Vertex{ glm::vec3(-0.5f,  0.5f, -0.5f),  Vector3::xAxis, white, glm::vec2(0.0f, 0.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f, -0.5f),  Vector3::xAxis, white, glm::vec2(0.0f, 1.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f,  0.5f),  Vector3::xAxis, white, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3(-0.5f,  0.5f,  0.5f),  Vector3::xAxis, white, glm::vec2(1.0f, 0.0f) },

		// bottom face
		Vertex{ glm::vec3( 0.5f, -0.5f, -0.5f), -Vector3::yAxis, white, glm::vec2(0.0f, 0.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f, -0.5f), -Vector3::yAxis, white, glm::vec2(1.0f, 0.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f,  0.5f), -Vector3::yAxis, white, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f, -0.5f,  0.5f), -Vector3::yAxis, white, glm::vec2(0.0f, 1.0f) },

		// top face
		Vertex{ glm::vec3( 0.5f,  0.5f, -0.5f),  Vector3::yAxis, white, glm::vec2(0.0f, 1.0f) },
		Vertex{ glm::vec3(-0.5f,  0.5f, -0.5f),  Vector3::yAxis, white, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3(-0.5f,  0.5f,  0.5f),  Vector3::yAxis, white, glm::vec2(1.0f, 0.0f) },
		Vertex{ glm::vec3( 0.5f,  0.5f,  0.5f),  Vector3::yAxis, white, glm::vec2(0.0f, 0.0f) }
	};

	// counter-clockwise order
	std::vector<uint32_t> indices =
	{
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

	return Mesh{ vertices, indices, UINT32_MAX, true };
}

Mesh PrimitiveMeshes::Quad()
{
	glm::vec3 white{ 1.f, 1.f, 1.f };

	std::vector<Vertex> vertices =
	{
		Vertex{ glm::vec3(  .5f, -.5f,  0.f ), -Vector3::zAxis, white, glm::vec2(1.f, 1.f) },
		Vertex{ glm::vec3( -.5f, -.5f,  0.f ), -Vector3::zAxis, white, glm::vec2(0.f, 1.f) },
		Vertex{ glm::vec3( -.5f,  .5f,  0.f ), -Vector3::zAxis, white, glm::vec2(0.f, 0.f) },
		Vertex{ glm::vec3(  .5f,  .5f,  0.f ), -Vector3::zAxis, white, glm::vec2(1.f, 0.f) }
	};

	std::vector<uint32_t> indices =
	{
		0, 1, 2,
		3, 0, 2
	};

	return Mesh{ vertices, indices, UINT32_MAX, true };
}
