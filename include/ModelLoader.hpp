#pragma once

#include<vector>

#include"gameobject/MeshObject.hpp"
#include<glm/matrix.hpp>
#include<vector>

#include"datatype/GameObject.hpp"
#include"render/Material.hpp"
#include"datatype/Mesh.hpp"

class ModelLoader
{
public:
	ModelLoader(const char* FilePath, GameObject* Parent);

	std::vector<GameObject*> LoadedObjs;

private:
	const char* File;

	std::vector<Mesh> Meshes;

	std::vector<glm::vec3> MeshTranslations;
	std::vector<glm::quat> MeshRotations;
	std::vector<glm::vec3> MeshScales;
	std::vector<glm::mat4> MeshMatrices;

	std::vector<std::vector<Texture*>> MeshTextures;

	void LoadMesh(uint32_t indMesh, glm::vec3 Translation, glm::quat Rotation, glm::vec3 Scale, glm::mat4 Transform);

	void TraverseNode(uint32_t NextNode, glm::mat4 Transform = glm::mat4(1.0f));

	std::vector<std::string> LoadedTexturePaths;
	std::vector<Texture*> LoadedTextures;

	std::vector<uint8_t> Data;

	std::vector<uint8_t> GetData();

	nlohmann::json JSONData;

	std::vector<float> GetFloats(nlohmann::json Accessor);
	std::vector<uint32_t> GetIndices(nlohmann::json Accessor);
	std::vector<Texture*> GetTextures();

	std::vector<Vertex> AssembleVertices(
		std::vector<glm::vec3> Positions,
		std::vector<glm::vec3> Normals,
		std::vector<glm::vec2> TextureUVs
	);

	std::vector<glm::vec2> GroupFloatsVec2(std::vector<float> Floats);
	std::vector<glm::vec3> GroupFloatsVec3(std::vector<float> Floats);
	std::vector<glm::vec4> GroupFloatsVec4(std::vector<float> Floats);
};
