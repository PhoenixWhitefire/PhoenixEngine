#pragma once

#include<vector>
#include<thread>

#include<nljson.h>

#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

#include<render/TextureManager.hpp>
#include<Engine.hpp>

#include<gameobject/Mesh3D.hpp>

class ModelLoader
{
public:
	ModelLoader(const char* FilePath, std::shared_ptr<GameObject> Parent, SDL_Window* Window);

	std::vector<std::shared_ptr<GameObject>> LoadedObjects;
private:
	const char* File;

	std::vector<Mesh> Meshes;

	std::vector<glm::vec3> MeshTranslations;
	std::vector<glm::quat> MeshRotations;
	std::vector<glm::vec3> MeshScales;
	std::vector<glm::mat4> MeshMatrices;

	std::vector<std::vector<Texture*>> MeshTextures;

	void LoadMesh(unsigned int indMesh, glm::vec3 Translation, glm::quat Rotation, glm::vec3 Scale, glm::mat4 matrix);

	void TraverseNode(unsigned int NextNode, glm::mat4 Matrix = glm::mat4(1.0f));

	std::vector<std::string> LoadedTexturePaths;
	std::vector<Texture*> LoadedTextures;

	std::vector<unsigned char> Data;

	std::vector<unsigned char> GetData();

	nlohmann::json JSONData;

	std::vector<float> GetFloats(nlohmann::json Accessor);
	std::vector<GLuint> GetIndices(nlohmann::json Accessor);
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
