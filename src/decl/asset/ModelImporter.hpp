#pragma once

#include "gameobject/MeshObject.hpp"

class ModelLoader
{
public:
	ModelLoader(const std::string& AssetPath, GameObject* Parent);

	std::vector<GameObject*> LoadedObjs;

private:
	struct MeshMaterial
	{
		enum class MaterialAlphaMode : uint8_t { Opaque, Mask, Blend };

		std::string Name{};

		glm::vec4 BaseColorFactor{ 1.f, 1.f, 1.f, 1.f };
		uint32_t BaseColorTexture{};

		float MetallicFactor{ 1.f };
		float RoughnessFactor{ 1.f };

		uint32_t MetallicRoughnessTexture{};
		uint32_t NormalTexture{};
		uint32_t EmissiveTexture{};

		glm::vec3 EmissiveFactor{ 0.f, 0.f, 0.f };

		bool DoubleSided = false;

		MaterialAlphaMode AlphaMode = MaterialAlphaMode::Opaque;
		float AlphaCutoff = .5f;
	};

	struct ModelNode
	{
		std::string Name;
		uint32_t Parent;
		bool IsContainerOnlyWithoutGeo = false; // nodes w/o meshes that just offset transformations

		Mesh Data;
		MeshMaterial Material;
		glm::mat4 Transform;
		glm::vec3 Scale{ 1.f, 1.f, 1.f };
	};

	ModelLoader() = delete;

	ModelNode m_LoadPrimitive(
		const nlohmann::json& MeshData,
		uint32_t PrimitiveIndex,
		const glm::mat4& Transform,
		const glm::vec3& Scale
	);

	void m_TraverseNode(uint32_t NextNode, uint32_t From, const glm::mat4& Transform = glm::mat4(1.0f));

	std::vector<int8_t> m_GetData();

	std::vector<float> m_GetFloats(const nlohmann::json& Accessor);
	std::vector<uint32_t> m_GetIndices(const nlohmann::json& Accessor);
	MeshMaterial m_GetMaterial(const nlohmann::json&);

	std::vector<Vertex> m_AssembleVertices(
		const std::vector<glm::vec3>& Positions,
		const std::vector<glm::vec3>& Normals,
		const std::vector<glm::vec2>& TextureUVs
	);

	std::vector<glm::vec2> m_GroupFloatsVec2(const std::vector<float>& Floats);
	std::vector<glm::vec3> m_GroupFloatsVec3(const std::vector<float>& Floats);
	std::vector<glm::vec4> m_GroupFloatsVec4(const std::vector<float>& Floats);

	std::string m_File;
	nlohmann::json m_JsonData;

	std::vector<ModelNode> m_Nodes;

	/*
	std::vector<Mesh> m_Meshes;
	std::vector<std::string> m_MeshNames;
	std::vector<uint32_t> m_MeshParents;
	std::vector<glm::mat4> m_MeshMatrices;
	std::vector<glm::vec3> m_MeshScales;
	std::vector<MeshMaterial> m_MeshMaterials;
	*/

	std::vector<int8_t> m_Data;
};
