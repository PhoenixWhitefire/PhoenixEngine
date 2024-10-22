#pragma once

#include"gameobject/MeshObject.hpp"

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

		bool DoubleSided = false;

		MaterialAlphaMode AlphaMode = MaterialAlphaMode::Opaque;
		float AlphaCutoff = .5f;
	};

	ModelLoader() = delete;

	void m_LoadMesh(uint32_t MeshIndex);

	void m_TraverseNode(uint32_t NextNode, glm::mat4 Transform = glm::mat4(1.0f));

	std::vector<uint8_t> m_GetData();

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

	std::vector<Mesh> m_Meshes;
	std::vector<std::string> m_MeshNames;
	std::vector<glm::mat4> m_MeshMatrices;
	std::vector<glm::vec3> m_MeshScales;

	std::vector<MeshMaterial> m_MeshMaterials;

	std::vector<uint8_t> m_Data;
};
