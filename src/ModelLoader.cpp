#include<thread>
#include<filesystem>
#include<nljson.hpp>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>

#include"ModelLoader.hpp"
#include"GlobalJsonConfig.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

ModelLoader::ModelLoader(const char* ModelPath, GameObject* Parent)
{
	this->File = "";

	std::string modelPathString = std::string(ModelPath);
	std::string gltfFilePath = modelPathString + "/scene.gltf";

	bool fileExists = true;
	std::string textData = FileRW::ReadFile(gltfFilePath, &fileExists);

	if (!fileExists)
	{
		Debug::Log("Failed to load model '" + modelPathString + "', `scene.gltf` not found.");
		return;
	}

	bool hasMaterialOverride = true;
	std::string overrideMaterial = FileRW::ReadFile(
		modelPathString + "/material.mtl",
		&hasMaterialOverride
	);
	std::string overrideMaterialPath = "";

	if (hasMaterialOverride)
	{
		// TODO
		// HACK HACK HACK
		// Copy the `material.mtl` into `resources/materials/`,
		// so that we can do `RenderMaterial::GetMaterial` instead of weird
		// file path `../` etc nonsense

		std::string resDir = EngineJsonConfig.value("ResourcesDirectory", "resources/");

		// MKDIR `resources/materials/models/`
		std::string modelsSubDirectory = resDir + "materials/models";
		bool createDirectorySuccess = !std::filesystem::is_directory(modelsSubDirectory)
						? std::filesystem::create_directory(modelsSubDirectory)
						: true;

		if (!createDirectorySuccess)
			throw(std::vformat(
				"Failed to `std::filesystem::create_directory` with path '{}'",
				std::make_format_args(modelsSubDirectory)
			));

		// resources/materials/models/crow.mtl
		overrideMaterialPath = "materials/"
								+ modelPathString
								+ ".mtl";

		std::string originPath = resDir + modelPathString + "/material.mtl";

		FileRW::WriteFile(overrideMaterialPath, overrideMaterial, true);
	}

	this->JSONData = nlohmann::json::parse(textData);
	this->File = gltfFilePath.c_str();

	this->Data = GetData();
	this->TraverseNode(0);

	for (int MeshIndex = 0; MeshIndex < this->Meshes.size(); MeshIndex++)
	{
		// TODO: cleanup code
		GameObject* M;
		Object_Base3D* m3d;
		Object_Mesh* mo;
		
		M = GameObject::CreateGameObject("Mesh");
		LoadedObjs.push_back(M);

		m3d = dynamic_cast<Object_Base3D*>(M);
		mo = dynamic_cast<Object_Mesh*>(M);

		mo->SetRenderMesh(this->Meshes[MeshIndex]);

		//mo->Orientation = this->MeshRotations[MeshIndex];

		m3d->Size = this->MeshScales[MeshIndex];

		if (hasMaterialOverride)
			// `materials/{models/[MODEL NAME]}.mtl`
			m3d->Material = RenderMaterial::GetMaterial(modelPathString);
		else
		{
			nlohmann::json materialJson{};

			for (int tix = 0; tix < this->MeshTextures[MeshIndex].size(); tix++)
			{
				Texture* tex = this->MeshTextures[MeshIndex][tix];

				switch (tex->Usage)
				{

				case (MaterialTextureType::Diffuse):
				{
					//m3d->Material->DiffuseTextures.push_back(tex);

					materialJson["albedo"] = std::string("/") + tex->ImagePath;

					break;
				}

				case (MaterialTextureType::Specular):
				{
					//m3d->Material->SpecularTextures.push_back(tex);

					materialJson["specular"] = std::string("/") + tex->ImagePath;

					break;
				}

				default:
					break;

				}
			}

			FileRW::WriteFile("materials/" + modelPathString + ".mtl", materialJson.dump(2), true);

			mo->Material = RenderMaterial::GetMaterial(modelPathString);
		}

		mo->Transform = this->MeshMatrices[MeshIndex];
		mo->Size = this->MeshScales[MeshIndex];
		
		//mo->Textures = this->MeshTextures[MeshIndex];

		if (Parent != nullptr)
			M->SetParent(Parent);
	}

	// TODO: fix matrices
	// hm, actually we already set the matrices in the for-loop above
	// something is breaking them the further they are from 0,0...
	/*
	momodel->MeshMatrices = std::vector(this->MeshMatrices);
	momodel->MeshRotations = std::vector(this->MeshRotations);
	momodel->MeshScales = std::vector(this->MeshScales);
	momodel->MeshTranslations = std::vector(this->MeshTranslations);
	*/
}

void ModelLoader::LoadMesh(uint32_t indMesh, glm::vec3 Translation, glm::quat Rotation, glm::vec3 Scale, glm::mat4 matrix)
{
	// Get all accessor indices
	uint32_t posAccInd = JSONData["meshes"][indMesh]["primitives"][0]["attributes"]["POSITION"];
	uint32_t normalAccInd = JSONData["meshes"][indMesh]["primitives"][0]["attributes"]["NORMAL"];
	uint32_t texAccInd = JSONData["meshes"][indMesh]["primitives"][0]["attributes"]["TEXCOORD_0"];
	uint32_t indAccInd = JSONData["meshes"][indMesh]["primitives"][0]["indices"];

	// Use accessor indices to get all vertices components
	std::vector<float> posVec = GetFloats(JSONData["accessors"][posAccInd]);
	std::vector<glm::vec3> positions = GroupFloatsVec3(posVec);
	std::vector<float> normalVec = GetFloats(JSONData["accessors"][normalAccInd]);
	std::vector<glm::vec3> normals = GroupFloatsVec3(normalVec);
	std::vector<float> texVec = GetFloats(JSONData["accessors"][texAccInd]);
	std::vector<glm::vec2> texUVs = GroupFloatsVec2(texVec);

	// Combine all the vertex components and also get the indices and textures
	std::vector<Vertex> vertices = AssembleVertices(positions, normals, texUVs);
	std::vector<uint32_t> indices = GetIndices(JSONData["accessors"][indAccInd]);
	std::vector<Texture*> textures = GetTextures();

	this->MeshTextures.push_back(textures);

	Mesh NewMesh = Mesh(vertices, indices);

	// TODO 30/08/2024 (also in Mesh.hpp)
	// Yes, this,
	// All of it
	// Kill it with hammers
	NewMesh.Matrix = matrix;
	NewMesh.Translation = Translation;
	NewMesh.Rotation = Rotation;
	NewMesh.Scale = Scale;

	// Combine the vertices, indices, and textures into a mesh
	Meshes.push_back(NewMesh);
}

void ModelLoader::TraverseNode(uint32_t nextNode, glm::mat4 matrix)
{
	// Current node
	nlohmann::json node = JSONData["nodes"][nextNode];

	// Get translation if it exists
	glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f);
	if (node.find("translation") != node.end())
	{
		float transValues[3]{};
		for (uint32_t i = 0; i < node["translation"].size(); i++)
			transValues[i] = (node["translation"][i]);
		translation = glm::make_vec3(transValues);
	}
	// Get quaternion if it exists
	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	if (node.find("rotation") != node.end())
	{
		float rotValues[4] =
		{
			node["rotation"][3],
			node["rotation"][0],
			node["rotation"][1],
			node["rotation"][2]
		};
		rotation = glm::make_quat(rotValues);
	}
	// Get scale if it exists
	glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
	if (node.find("scale") != node.end())
	{
		float scaleValues[3]{};
		for (uint32_t i = 0; i < node["scale"].size(); i++)
			scaleValues[i] = (node["scale"][i]);
		scale = glm::make_vec3(scaleValues);
	}
	// Get matrix if it exists
	glm::mat4 matNode = glm::mat4(1.0f);
	if (node.find("matrix") != node.end())
	{
		float matValues[16]{};
		for (uint32_t i = 0; i < node["matrix"].size(); i++)
			matValues[i] = (node["matrix"][i]);
		matNode = glm::make_mat4(matValues);
	}

	// Initialize matrices
	glm::mat4 trans = glm::mat4(1.0f);
	glm::mat4 rot = glm::mat4(1.0f);
	glm::mat4 sca = glm::mat4(1.0f);

	// Use translation, rotation, and scale to change the initialized matrices
	trans = glm::translate(trans, translation);
	rot = glm::mat4_cast(rotation);
	sca = glm::scale(sca, scale);

	// Multiply all matrices together
	glm::mat4 matNextNode = matrix * matNode * trans * rot * sca;

	// Check if the node contains a mesh and if it does load it
	if (node.find("mesh") != node.end())
	{
		MeshTranslations.push_back(translation);
		MeshRotations.push_back(rotation);
		MeshScales.push_back(scale);
		MeshMatrices.push_back(matNextNode);

		LoadMesh(node["mesh"], translation, rotation, scale, matNextNode);
	}

	// Check if the node has children, and if it does, apply this function to them with the matNextNode
	if (node.find("children") != node.end())
	{
		for (uint32_t i = 0; i < node["children"].size(); i++) {
			TraverseNode(node["children"][i], matNextNode);
		}
	}
}

std::vector<uint8_t> ModelLoader::GetData()
{
	// Create a place to store the raw text, and get the uri of the .bin file
	std::string bytesText;
	std::string uri = JSONData["buffers"][0]["uri"];

	// Store raw text data into bytesText
	std::string fileStr = std::string(File);
	std::string fileDirectory = fileStr.substr(0, fileStr.find_last_of('/') + 1);
	bytesText = FileRW::ReadFile((fileDirectory + uri));

	// Transform the raw text data into bytes and put them in a vector
	std::vector<uint8_t> data(bytesText.begin(), bytesText.end());
	return data;
}

std::vector<float> ModelLoader::GetFloats(nlohmann::json accessor)
{
	std::vector<float> floatVec;

	// Get properties from the accessor
	uint32_t buffViewInd = accessor.value("bufferView", 1);
	uint32_t count = accessor["count"];
	uint32_t accByteOffset = accessor.value("byteOffset", 0);
	std::string type = accessor["type"];

	// Get properties from the bufferView
	nlohmann::json bufferView = JSONData["bufferViews"][buffViewInd];
	uint32_t byteOffset = bufferView["byteOffset"];

	// Interpret the type and store it into numPerVert
	uint32_t numPerVert;
	if (type == "SCALAR") numPerVert = 1;
	else if (type == "VEC2") numPerVert = 2;
	else if (type == "VEC3") numPerVert = 3;
	else if (type == "VEC4") numPerVert = 4;
	else throw("Could not decode GLTF model: Type is not handled (not SCALAR, VEC2, VEC3, or VEC4)");

	// Go over all the bytes in the data at the correct place using the properties from above
	uint32_t beginningOfData = byteOffset + accByteOffset;
	uint32_t lengthOfData = count * 4 * numPerVert;
	for (uint32_t i = beginningOfData; i < beginningOfData + lengthOfData; i)
	{
		uint8_t bytes[] = { Data[i++], Data[i++], Data[i++], Data[i++] };
		float value;
		std::memcpy(&value, bytes, sizeof(float));
		floatVec.push_back(value);
	}

	return floatVec;
}

std::vector<uint32_t> ModelLoader::GetIndices(nlohmann::json accessor)
{
	std::vector<uint32_t> indices;

	// Get properties from the accessor
	uint32_t buffViewInd = accessor.value("bufferView", 0);
	uint32_t count = accessor["count"];
	uint32_t accByteOffset = accessor.value("byteOffset", 0);
	uint32_t componentType = accessor["componentType"];

	// Get properties from the bufferView
	nlohmann::json bufferView = JSONData["bufferViews"][buffViewInd];
	uint32_t byteOffset = bufferView.value("byteOffset", 0);

	// Get indices with regards to their type: uint32_t, uint16_t, or short
	uint32_t beginningOfData = byteOffset + accByteOffset;
	if (componentType == 5125)
	{
		for (uint32_t i = beginningOfData; i < byteOffset + accByteOffset + count * 4; i)
		{
			uint8_t bytes[] = { Data[i++], Data[i++], Data[i++], Data[i++] };
			uint32_t value;
			std::memcpy(&value, bytes, sizeof(uint32_t));
			indices.push_back(value);
		}
	}
	else if (componentType == 5123)
	{
		for (uint32_t i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
		{
			uint8_t bytes[] = { Data[i++], Data[i++] };
			uint16_t value;
			std::memcpy(&value, bytes, sizeof(uint16_t));
			indices.push_back(value);
		}
	}
	else if (componentType == 5122)
	{
		for (uint32_t i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
		{
			uint8_t bytes[] = { Data[i++], Data[i++] };
			short value;
			std::memcpy(&value, bytes, sizeof(uint16_t));
			indices.push_back(value);
		}
	}

	return indices;
}

std::vector<Texture*> ModelLoader::GetTextures()
{
	std::vector<Texture*> textures;

	std::string fileStr = std::string(File);
	std::string fileDirectory = fileStr.substr(0, fileStr.find_last_of('/') + 1);

	// Go over all images
	for (uint32_t i = 0; i < JSONData["images"].size(); i++)
	{
		// uri of current texture
		std::string texPath = JSONData["images"][i]["uri"];

		std::string FullTexturePath = fileDirectory + texPath;

		// Check if the texture has already been loaded
		bool skip = false;
		uint32_t OldTextureIndex = 0;

		for (uint32_t j = 0; j < LoadedTexturePaths.size(); j++)
		{
			if (LoadedTexturePaths[j] == texPath)
			{
				textures.push_back(LoadedTextures[j]);
				skip = true;
				OldTextureIndex = j;
				break;
			}
		}

		// If the texture has been loaded, skip this
		if (!skip)
		{
			MaterialTextureType TexType = MaterialTextureType::NotAssigned;

			// TODO: set usage based on how it's defined in the file itself
			if (texPath.find("baseColor") != std::string::npos || texPath.find("diffuse") != std::string::npos)
				TexType = MaterialTextureType::Diffuse;
			else if (texPath.find("metallicRoughness") != std::string::npos || texPath.find("specular") != std::string::npos)
				TexType = MaterialTextureType::Specular;
			else
			{
				Debug::Log(std::vformat(
					"Could not determine how texture '{}' is meant to be used!",
					std::make_format_args(texPath)
				));

				continue;
			}

			Texture* texture = TextureManager::Get()->LoadTextureFromPath(FullTexturePath);

			texture->Usage = TexType;

			LoadedTextures.push_back(texture);
			LoadedTexturePaths.push_back(texPath);

			textures.push_back(texture);
		}
		else
			textures.push_back(LoadedTextures[OldTextureIndex]);
	}

	return textures;
}

std::vector<Vertex> ModelLoader::AssembleVertices
(
	std::vector<glm::vec3> positions,
	std::vector<glm::vec3> normals,
	std::vector<glm::vec2> texUVs
)
{
	std::vector<Vertex> vertices;
	for (int i = 0; i < positions.size(); i++)
	{
		vertices.push_back
		(
			Vertex
			{
				positions[i],
				normals[i],
				glm::vec3(1.0f, 1.0f, 1.0f),
				texUVs[i]
			}
		);
	}
	return vertices;
}

std::vector<glm::vec2> ModelLoader::GroupFloatsVec2(std::vector<float> floatVec)
{
	std::vector<glm::vec2> vectors;
	for (int i = 0; i < floatVec.size(); i)
	{
		vectors.push_back(glm::vec2(floatVec[i++], floatVec[i++]));
	}
	return vectors;
}

std::vector<glm::vec3> ModelLoader::GroupFloatsVec3(std::vector<float> floatVec)
{
	std::vector<glm::vec3> vectors;
	for (int i = 0; i < floatVec.size(); i)
		vectors.push_back(glm::vec3(floatVec[i++], floatVec[i++], floatVec[i++]));

	return vectors;
}
std::vector<glm::vec4> ModelLoader::GroupFloatsVec4(std::vector<float> floatVec)
{
	std::vector<glm::vec4> vectors;
	for (int i = 0; i < floatVec.size(); i)
		vectors.push_back(glm::vec4(floatVec[i++], floatVec[i++], floatVec[i++], floatVec[i++]));

	return vectors;
}
