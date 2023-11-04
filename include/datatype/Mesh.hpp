#pragma once

#include<vector>

#include<render/TextureManager.hpp>
#include"Buffer.hpp"

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

enum class FaceCullingMode { None, BackFace, FrontFace };

class Mesh {
	public:
		Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices, glm::vec3 verticesColor);
		Mesh(std::vector<Vertex>& vertices, std::vector<GLuint>& indices);
		Mesh();

		std::vector<Vertex> Vertices;
		std::vector<GLuint> Indices;

		FaceCullingMode CulledFace = FaceCullingMode::BackFace;

		glm::mat4 Matrix = glm::mat4(1.0f);
		glm::vec3 Translation = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		glm::vec3 Scale = glm::vec3(1.0f, 1.0f, 1.0f);

		bool ShouldTextureRepeat = true;
};
