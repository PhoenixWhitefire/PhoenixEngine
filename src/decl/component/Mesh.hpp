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

	uint32_t RenderMeshId = UINT32_MAX;
	uint32_t MaterialId = UINT32_MAX;

	float Transparency = 0.f;
	float MetalnessFactor = 1.f;
	float RoughnessFactor = 1.f;

	Color Tint = { 1.f, 1.f, 1.f };

	std::string Asset = "!Cube";
	uint32_t GpuSkinningBuffer = UINT32_MAX;

	uint32_t ComponentId = UINT32_MAX;

	ObjectRef Object;
	FaceCullingMode FaceCulling = FaceCullingMode::BackFace;
	bool CastsShadows = true;

	bool Valid = true;
};

class MeshComponentManager : public ComponentManager<EcMesh, MeshComponentManager>
{
public:
    uint32_t CreateComponent(GameObject* Object) override;
	void DeleteComponent(uint32_t Id) override;
	Reflection::GenericValue GetDefaultPropertyValue(const std::string_view& Property) override;
	Reflection::GenericValue GetDefaultPropertyValue(const Reflection::PropertyDescriptor* Property) override;
	const Reflection::StaticPropertyMap& GetProperties() override;

	std::unordered_map<std::string, std::vector<uint32_t>> FreeSkinnedMeshPseudoAssets;
};
