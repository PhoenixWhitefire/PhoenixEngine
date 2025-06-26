#pragma once

#include <unordered_set>
#include <stdint.h>
#include <glm/mat4x4.hpp>

#include "component/Mesh.hpp"
#include "Memory.hpp"

struct RenderItem
{
	uint32_t RenderMeshId{};
	glm::mat4 Transform{};
	glm::vec3 Size;
	uint32_t MaterialId{};
	glm::vec3 TintColor;
	float Transparency{};
	float MetallnessFactor{};
	float RoughnessFactor{};

	FaceCullingMode FaceCulling = FaceCullingMode::BackFace;
	bool CastsShadows = false;
};

enum class LightType : uint8_t { Directional, Point, Spot };

struct LightItem
{
	LightType Type = LightType::Point;
	bool Shadows = false;

	// acts as the direction for Directional Lights
	glm::vec3 Position;
	glm::vec3 LightColor;

	// spot & pointlights
	float Range = 60.f;
	// spotlights
	float Angle = 60.f;
};

struct Scene
{
	Memory::vector<RenderItem, MEMCAT(Rendering)> RenderList;
	Memory::vector<LightItem, MEMCAT(Rendering)> LightingList;

	Memory::unordered_set<uint32_t, MEMCAT(Rendering)> UsedShaders;
};
