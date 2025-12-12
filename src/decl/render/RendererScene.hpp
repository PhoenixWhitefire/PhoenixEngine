#pragma once

#include <unordered_set>
#include <stdint.h>
#include <glm/mat4x4.hpp>

#include "component/Mesh.hpp"
#include "Memory.hpp"

struct RenderItem
{
	uint32_t RenderMeshId = UINT32_MAX;
	glm::mat4 Transform;
	glm::vec3 Size;
	uint32_t MaterialId = UINT32_MAX;
	glm::vec3 TintColor;
	float Transparency = 0.f;
	float MetallnessFactor = 0.f;
	float RoughnessFactor = 0.f;

	FaceCullingMode FaceCulling = FaceCullingMode::BackFace;
	bool CastsShadows = false;
};

enum class LightType : uint8_t { Directional, Point, Spot };

struct LightItem
{
	// acts as the direction for Directional Lights
	glm::vec3 Position;
	glm::vec3 LightColor;

	// spot & pointlights
	float Range = 60.f;
	// spotlights
	glm::vec3 SpotLightDirection;
	float Angle = 60.f;

	LightType Type = LightType::Point;
	bool Shadows = false;
};

struct Scene
{
	hx::vector<RenderItem, MEMCAT(Rendering)> RenderList;
	hx::vector<LightItem, MEMCAT(Rendering)> LightingList;

	hx::unordered_set<uint32_t, MEMCAT(Rendering)> UsedShaders;
};
