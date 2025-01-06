#pragma once

#include <vector>
#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

struct Vertex
{
	glm::vec3 Position;
	glm::vec3 Normal;
	// RGBA
	glm::vec4 Paint;
	glm::vec2 TextureUV;
	// skinned mesh animation, 4 bones max per vert
	std::array<uint8_t, 4> InfluencingJoints = { UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX };
	std::array<float, 4> JointWeights = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
};

struct Bone
{
	glm::mat4 Transform{ 1.f };
	glm::vec3 Scale{ 1.f };
};

struct Mesh
{
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;
	std::vector<Bone> Bones;

	uint32_t GpuId = UINT32_MAX;
	bool MeshDataPreserved = false;
};
