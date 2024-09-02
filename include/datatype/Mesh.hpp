#pragma once

#include<vector>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include"Buffer.hpp"

struct Mesh
{
	Mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, glm::vec3 verticesColor);
	Mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
	Mesh();

	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;

	// TODO 30/08/2024 (also in ModelLoader.cpp)
	// Yes, this,
	// All of it
	// Kill it with hammers
	glm::mat4 Matrix = glm::mat4(1.0f);
	glm::vec3 Translation = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 Scale = glm::vec3(1.0f, 1.0f, 1.0f);
};
