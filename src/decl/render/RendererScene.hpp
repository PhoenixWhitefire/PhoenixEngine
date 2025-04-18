#pragma once

#include <unordered_set>
#include <stdint.h>
#include <glm/mat4x4.hpp>

#include "component/Mesh.hpp"

struct RenderItem
{
	uint32_t RenderMeshId{};
	glm::mat4 Transform{};
	glm::vec3 Size;
	uint32_t MaterialId{};
	Color TintColor;
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
	Vector3 Position;
	Color LightColor;

	// spot & pointlights
	float Range = 60.f;
	// spotlights
	float Angle = 60.f;
};

struct Scene
{
	std::vector<RenderItem> RenderList;
	std::vector<LightItem> LightingList;

	std::unordered_set<uint32_t> UsedShaders;
};
