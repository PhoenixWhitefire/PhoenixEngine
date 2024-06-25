#pragma once

#include<vector>

#include"Buffer.hpp"

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

enum class FaceCullingMode { None, BackFace, FrontFace };

struct Mesh
{
	Mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, glm::vec3 verticesColor);
	Mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
	Mesh();

	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;

	FaceCullingMode CulledFace = FaceCullingMode::BackFace;

	glm::mat4 Matrix = glm::mat4(1.0f);
	glm::vec3 Translation = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 Scale = glm::vec3(1.0f, 1.0f, 1.0f);

	bool ShouldTextureRepeat = true;
};
