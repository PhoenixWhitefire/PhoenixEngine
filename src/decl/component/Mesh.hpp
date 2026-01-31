#pragma once

#include <string>
#include <glm/vec3.hpp>

#include "datatype/GameObject.hpp"
#include "datatype/ComponentBase.hpp"
#include "datatype/Color.hpp"

enum class FaceCullingMode : uint8_t { None, BackFace, FrontFace };

struct EcMesh : public Component<EntityComponent::Mesh>
{
	void SetRenderMesh(const std::string_view&);

	bool CastsShadows = true;

	FaceCullingMode FaceCulling = FaceCullingMode::BackFace;

	uint32_t RenderMeshId = UINT32_MAX;
	uint32_t MaterialId = UINT32_MAX;

	float Transparency = 0.f;
	float MetallnessFactor = 1.f;
	float RoughnessFactor = 1.f;

	Color Tint = { 1.f, 1.f, 1.f };

	std::string Asset = "!Cube";
	uint32_t GpuSkinningBuffer = UINT32_MAX;

	uint32_t ComponentId = UINT32_MAX;

	ObjectRef Object;
	bool Valid = true;
};
