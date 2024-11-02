#pragma once

#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

struct Vertex
{
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec3 Color;
	glm::vec2 TextureUV;
};

struct Mesh
{
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;

	uint32_t GpuId = UINT32_MAX;
	bool MeshDataPreserved = false;
};
