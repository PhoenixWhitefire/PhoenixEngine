#include<filesystem>
#include<glm/gtc/type_ptr.hpp>

#include"ModelLoader.hpp"
#include"GlobalJsonConfig.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

enum class MaterialTextureType { Diffuse, Specular };

ModelLoader::ModelLoader(const std::string& AssetPath, GameObject* Parent)
{
	m_File = "";

	std::string gltfFilePath = AssetPath + "/scene.gltf";

	m_File = gltfFilePath;

	bool fileExists = true;
	std::string textData = FileRW::ReadFile(gltfFilePath, &fileExists);

	if (!fileExists)
	{
		Debug::Log("Failed to load model '" + AssetPath + "', `scene.gltf` not found.");
		return;
	}

	bool hasMaterialOverride = true;
	std::string overrideMaterial = FileRW::ReadFile(
		AssetPath + "/material.mtl",
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
								+ AssetPath
								+ ".mtl";

		std::string originPath = resDir + AssetPath + "/material.mtl";

		nlohmann::json overrideMaterialJson = nlohmann::json::parse(overrideMaterial);
		// 05/09/2024
		// For the Material Editor to know what file to save to
		overrideMaterialJson["originalPath"] = AssetPath + "material.mtl";
		overrideMaterial = overrideMaterialJson.dump(2);

		FileRW::WriteFile(overrideMaterialPath, overrideMaterial, true);
	}

	m_JsonData = nlohmann::json::parse(textData);

	m_Data = m_GetData();
	m_TraverseNode(0);

	for (int MeshIndex = 0; MeshIndex < m_Meshes.size(); MeshIndex++)
	{
		// TODO: cleanup code
		Object_Mesh* mesh = dynamic_cast<Object_Mesh*>(GameObject::Create("Mesh"));
		
		LoadedObjs.push_back(mesh);

		mesh->SetRenderMesh(m_Meshes[MeshIndex]);
		mesh->Transform = m_MeshMatrices[MeshIndex];

		//mo->Orientation = this->MeshRotations[MeshIndex];

		mesh->Size = m_MeshScales[MeshIndex];

		if (hasMaterialOverride)
			// `materials/{models/[MODEL NAME]}.mtl`
			mesh->Material = RenderMaterial::GetMaterial(AssetPath);
		else
		{
			nlohmann::json materialJson{};

			for (auto& it : m_MeshTextures)
			{
				Texture* tex = TextureManager::Get()->GetTextureResource(it.second);

				std::string key = it.first == MaterialTextureType::Diffuse ? "albedo" : "specular";

				materialJson[key] = tex->ImagePath;
			}

			FileRW::WriteFile("materials/" + AssetPath + ".mtl", materialJson.dump(2), true);

			mesh->Material = RenderMaterial::GetMaterial(AssetPath);
		}

		mesh->Transform = m_MeshMatrices[MeshIndex];
		mesh->Size = m_MeshScales[MeshIndex];
		
		//mo->Textures = this->MeshTextures[MeshIndex];

		if (Parent != nullptr)
			mesh->SetParent(Parent);
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

void ModelLoader::m_LoadMesh(uint32_t MeshIndex)
{
	nlohmann::json mainPrims = m_JsonData["meshes"][MeshIndex]["primitives"][0];

	// Get all accessor indices
	uint32_t posAccInd = mainPrims["attributes"]["POSITION"];
	uint32_t normalAccInd = mainPrims["attributes"]["NORMAL"];
	uint32_t texAccInd = mainPrims["attributes"]["TEXCOORD_0"];
	uint32_t indAccInd = mainPrims["indices"];

	// Use accessor indices to get all vertices components
	std::vector<float> posVec = m_GetFloats(m_JsonData["accessors"][posAccInd]);
	std::vector<glm::vec3> positions = m_GroupFloatsVec3(posVec);
	std::vector<float> normalVec = m_GetFloats(m_JsonData["accessors"][normalAccInd]);
	std::vector<glm::vec3> normals = m_GroupFloatsVec3(normalVec);
	std::vector<float> texVec = m_GetFloats(m_JsonData["accessors"][texAccInd]);
	std::vector<glm::vec2> texUVs = m_GroupFloatsVec2(texVec);

	// Combine all the vertex components and also get the indices and textures
	std::vector<Vertex> vertices = m_AssembleVertices(positions, normals, texUVs);
	std::vector<uint32_t> indices = m_GetIndices(m_JsonData["accessors"][indAccInd]);
	
	m_MeshTextures = m_GetTextures();

	Mesh newMesh{ vertices, indices };

	// Combine the vertices, indices, and textures into a mesh
	m_Meshes.push_back(newMesh);
}

void ModelLoader::m_TraverseNode(uint32_t nextNode, glm::mat4 matrix)
{
	// Current node
	nlohmann::json node = m_JsonData["nodes"][nextNode];

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
	glm::mat4 trans = glm::mat4(1.0f); // 14/09/2024 har har
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
		//m_MeshTranslations.push_back(translation);
		//m_MeshRotations.push_back(rotation);
		m_MeshScales.push_back(scale);
		m_MeshMatrices.push_back(matNextNode);

		m_LoadMesh(node["mesh"]);
	}

	// Check if the node has children, and if it does, apply this function to them with the matNextNode
	if (node.find("children") != node.end())
	{
		for (nlohmann::json subnode : node["children"])
			m_TraverseNode(subnode, matNextNode);
	}
}

std::vector<uint8_t> ModelLoader::m_GetData()
{
	// Create a place to store the raw text, and get the uri of the .bin file
	std::string bytesText;
	std::string uri = m_JsonData["buffers"][0]["uri"];

	// Store raw text data into bytesText
	std::string fileStr = std::string(m_File);
	std::string fileDirectory = fileStr.substr(0, fileStr.find_last_of('/') + 1);
	bytesText = FileRW::ReadFile((fileDirectory + uri));

	// Transform the raw text data into bytes and put them in a vector
	std::vector<uint8_t> data(bytesText.begin(), bytesText.end());
	return data;
}

std::vector<float> ModelLoader::m_GetFloats(nlohmann::json accessor)
{
	std::vector<float> floatVec;

	// Get properties from the accessor
	uint32_t buffViewInd = accessor.value("bufferView", 1);
	uint32_t count = accessor["count"];
	uint32_t accByteOffset = accessor.value("byteOffset", 0);
	std::string type = accessor["type"];

	// Get properties from the bufferView
	nlohmann::json bufferView = m_JsonData["bufferViews"][buffViewInd];
	uint32_t byteOffset = bufferView["byteOffset"];

	// Interpret the type and store it into numPerVert
	uint32_t numPerVert;
	if (type == "SCALAR")
		numPerVert = 1;
	else if (type == "VEC2")
		numPerVert = 2;
	else if (type == "VEC3")
		numPerVert = 3;
	else if (type == "VEC4")
		numPerVert = 4;
	else
		throw("Could not decode GLTF model: Type is not handled (not SCALAR, VEC2, VEC3, or VEC4)");

	// Go over all the bytes in the data at the correct place using the properties from above
	uint32_t beginningOfData = byteOffset + accByteOffset;
	uint32_t lengthOfData = count * 4 * numPerVert;
	for (uint32_t i = beginningOfData; i < beginningOfData + lengthOfData; i)
	{
		uint8_t bytes[] = { m_Data[i++], m_Data[i++], m_Data[i++], m_Data[i++] };
		float value;
		std::memcpy(&value, bytes, sizeof(float));
		floatVec.push_back(value);
	}

	return floatVec;
}

std::vector<uint32_t> ModelLoader::m_GetIndices(nlohmann::json accessor)
{
	std::vector<uint32_t> indices;

	// Get properties from the accessor
	uint32_t buffViewInd = accessor.value("bufferView", 0);
	uint32_t count = accessor["count"];
	uint32_t accByteOffset = accessor.value("byteOffset", 0);
	uint32_t componentType = accessor["componentType"];

	// Get properties from the bufferView
	nlohmann::json bufferView = m_JsonData["bufferViews"][buffViewInd];
	uint32_t byteOffset = bufferView.value("byteOffset", 0);

	// Get indices with regards to their type: uint32_t, uint16_t, or short
	uint32_t beginningOfData = byteOffset + accByteOffset;
	if (componentType == 5125)
	{
		for (uint32_t i = beginningOfData; i < byteOffset + accByteOffset + count * 4; i)
		{
			uint8_t bytes[] = { m_Data[i++], m_Data[i++], m_Data[i++], m_Data[i++] };
			uint32_t value;
			std::memcpy(&value, bytes, sizeof(uint32_t));
			indices.push_back(value);
		}
	}
	else if (componentType == 5123)
	{
		for (uint32_t i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
		{
			uint8_t bytes[] = { m_Data[i++], m_Data[i++] };
			uint16_t value;
			std::memcpy(&value, bytes, sizeof(uint16_t));
			indices.push_back(value);
		}
	}
	else if (componentType == 5122)
	{
		for (uint32_t i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
		{
			uint8_t bytes[] = { m_Data[i++], m_Data[i++] };
			short value;
			std::memcpy(&value, bytes, sizeof(uint16_t));
			indices.push_back(value);
		}
	}

	return indices;
}

std::unordered_map<ModelLoader::MaterialTextureType, uint32_t> ModelLoader::m_GetTextures()
{
	TextureManager* texManager = TextureManager::Get();

	std::unordered_map<MaterialTextureType, uint32_t> textures;

	std::string fileDirectory = m_File.substr(0, m_File.find_last_of('/') + 1);

	// Go over all images
	for (nlohmann::json image : m_JsonData["images"])
	{
		// uri of current texture
		std::string texPath = image["uri"];
		std::string fullTexturePath = fileDirectory + texPath;

		MaterialTextureType texType{};

		// TODO: set usage based on how it's defined in the file itself
		if (texPath.find("baseColor") != std::string::npos || texPath.find("diffuse") != std::string::npos)
			texType = MaterialTextureType::Diffuse;

		else if (texPath.find("metallicRoughness") != std::string::npos || texPath.find("specular") != std::string::npos)
			texType = MaterialTextureType::Specular;

		else
		{
			Debug::Log(std::vformat(
				"Could not determine how texture '{}' is meant to be used!",
				std::make_format_args(texPath)
			));

			continue;
		}

		uint32_t texture = texManager->LoadTextureFromPath(fullTexturePath);

		textures.insert(std::pair(texType, texture));
	}

	return textures;
}

std::vector<Vertex> ModelLoader::m_AssembleVertices
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

std::vector<glm::vec2> ModelLoader::m_GroupFloatsVec2(std::vector<float> floatVec)
{
	std::vector<glm::vec2> vectors;

	for (int i = 0; i < floatVec.size(); i)
	{
		vectors.push_back(
			glm::vec2(
				floatVec[i++],
				floatVec[i++])
		);
	}
	return vectors;
}

std::vector<glm::vec3> ModelLoader::m_GroupFloatsVec3(std::vector<float> floatVec)
{
	std::vector<glm::vec3> vectors;

	for (int i = 0; i < floatVec.size(); i)
		vectors.push_back(
			glm::vec3(
				floatVec[i++],
				floatVec[i++],
				floatVec[i++])
		);

	return vectors;
}
std::vector<glm::vec4> ModelLoader::m_GroupFloatsVec4(std::vector<float> floatVec)
{
	std::vector<glm::vec4> vectors;

	for (int i = 0; i < floatVec.size(); i)
		vectors.push_back(
			glm::vec4(
				floatVec[i++],
				floatVec[i++],
				floatVec[i++],
				floatVec[i++])
		);

	return vectors;
}
