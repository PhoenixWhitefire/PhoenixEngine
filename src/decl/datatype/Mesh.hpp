#pragma once

#include<vector>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include"Buffer.hpp"

struct Mesh
{
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;
};
