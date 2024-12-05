#pragma once

#include <unordered_set>
#include <stdint.h>
#include <glm/mat4x4.hpp>

#include "gameobject/Base3D.hpp"

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
};

enum class LightType : uint8_t { Directional, Point, Spot };

struct LightItem
{
	LightType Type = LightType::Point;

	Vector3 Position;
	Color LightColor;
	float Range = 60.f;

	glm::mat4 ShadowMapProjection{};
	bool HasShadowMap = false;
	int ShadowMapIndex{};
};

struct Scene
{
	std::vector<RenderItem> RenderList;
	std::vector<LightItem> LightingList;

	std::unordered_set<uint32_t> UsedShaders;
};
