#include<BaseMeshes.hpp>

Mesh* BaseMeshes::m_Cube = nullptr;

Mesh* BaseMeshes::Cube() {
	if (BaseMeshes::m_Cube != nullptr) {
		return BaseMeshes::m_Cube;
	}

	// white vertex color
	glm::vec3 White(1.0f, 1.0f, 1.0f);

	std::vector<Vertex> Vertices =
	{
		// front face
		Vertex{ glm::vec3(-0.5f,  0.5f, -0.5f),  Vector3::FORWARD, White, glm::vec2(0.0f, 0.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f, -0.5f),  Vector3::FORWARD, White, glm::vec2(0.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f, -0.5f, -0.5f),  Vector3::FORWARD, White, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f,  0.5f, -0.5f),  Vector3::FORWARD, White, glm::vec2(1.0f, 0.0f) },

		// back face
		Vertex{ glm::vec3(-0.5f,  0.5f,  0.5f), Vector3::BACKWARD, White, glm::vec2(0.0f, 0.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f,  0.5f), Vector3::BACKWARD, White, glm::vec2(0.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f, -0.5f,  0.5f), Vector3::BACKWARD, White, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f,  0.5f,  0.5f), Vector3::BACKWARD, White, glm::vec2(1.0f, 0.0f) },

		// right face
		// it is at this point that i am wondering if i can just read a .obj cube from blender or smthing
		// instead of doing this by hand and by struggling to imaging where the vertices are in my head
		Vertex{ glm::vec3( 0.5f,  0.5f, -0.5f), Vector3::RIGHT, White, glm::vec2(0.0f, 0.0f) },
		Vertex{ glm::vec3( 0.5f, -0.5f, -0.5f), Vector3::RIGHT, White, glm::vec2(0.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f, -0.5f,  0.5f), Vector3::RIGHT, White, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f,  0.5f,  0.5f), Vector3::RIGHT, White, glm::vec2(1.0f, 0.0f) },

		// left face
		Vertex{ glm::vec3(-0.5f,  0.5f, -0.5f), Vector3::LEFT, White, glm::vec2(0.0f, 0.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f, -0.5f), Vector3::LEFT, White, glm::vec2(0.0f, 1.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f,  0.5f), Vector3::LEFT, White, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3(-0.5f,  0.5f,  0.5f), Vector3::LEFT, White, glm::vec2(1.0f, 0.0f) },

		// bottom face
		Vertex{ glm::vec3( 0.5f, -0.5f, -0.5f), Vector3::DOWN, White, glm::vec2(0.0f, 0.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f, -0.5f), Vector3::DOWN, White, glm::vec2(0.0f, 1.0f) },
		Vertex{ glm::vec3(-0.5f, -0.5f,  0.5f), Vector3::DOWN, White, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f, -0.5f,  0.5f), Vector3::DOWN, White, glm::vec2(1.0f, 0.0f) },

		// top face
		Vertex{ glm::vec3( 0.5f,  0.5f, -0.5f), Vector3::UP, White, glm::vec2(0.0f, 0.0f) },
		Vertex{ glm::vec3(-0.5f,  0.5f, -0.5f), Vector3::UP, White, glm::vec2(0.0f, 1.0f) },
		Vertex{ glm::vec3(-0.5f,  0.5f,  0.5f), Vector3::UP, White, glm::vec2(1.0f, 1.0f) },
		Vertex{ glm::vec3( 0.5f,  0.5f,  0.5f), Vector3::UP, White, glm::vec2(1.0f, 0.0f) }
	};

	// counter-clockwise order
	std::vector<GLuint> Indices = {
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

	BaseMeshes::m_Cube = new Mesh(Vertices, Indices);

	return BaseMeshes::m_Cube;
}
