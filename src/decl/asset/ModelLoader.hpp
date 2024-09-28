#pragma once

#include"gameobject/MeshObject.hpp"

class ModelLoader
{
public:
	ModelLoader(const std::string& AssetPath, GameObject* Parent);

	std::vector<GameObject*> LoadedObjs;

private:
	enum class MaterialTextureType { Diffuse, Specular };

	ModelLoader() = delete;

	void m_LoadMesh(uint32_t MeshIndex);

	void m_TraverseNode(uint32_t NextNode, glm::mat4 Transform = glm::mat4(1.0f));

	std::vector<uint8_t> m_GetData();

	std::vector<float> m_GetFloats(nlohmann::json Accessor);
	std::vector<uint32_t> m_GetIndices(nlohmann::json Accessor);
	std::unordered_map<MaterialTextureType, uint32_t> m_GetTextures();

	std::vector<Vertex> m_AssembleVertices(
		std::vector<glm::vec3> Positions,
		std::vector<glm::vec3> Normals,
		std::vector<glm::vec2> TextureUVs
	);

	std::vector<glm::vec2> m_GroupFloatsVec2(std::vector<float> Floats);
	std::vector<glm::vec3> m_GroupFloatsVec3(std::vector<float> Floats);
	std::vector<glm::vec4> m_GroupFloatsVec4(std::vector<float> Floats);

	std::string m_File;
	nlohmann::json m_JsonData;

	std::vector<Mesh> m_Meshes;
	std::vector<glm::mat4> m_MeshMatrices;
	std::vector<glm::vec3> m_MeshScales;

	std::unordered_map<MaterialTextureType, uint32_t> m_MeshTextures;

	std::vector<uint8_t> m_Data;
};
