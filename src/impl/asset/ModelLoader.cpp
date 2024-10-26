#include <filesystem>
#include <glm/gtc/type_ptr.hpp>

#include "asset/ModelLoader.hpp"
#include "asset/TextureManager.hpp"
#include "asset/MeshProvider.hpp"
#include "GlobalJsonConfig.hpp"
#include "FileRW.hpp"
#include "Debug.hpp"

static uint32_t readU32(const std::string& str, size_t offset)
{
	std::string substr = str.substr(offset, 4);

	std::vector<uint8_t> bytes(substr.begin(), substr.end());
	uint32_t u32{};

	std::memcpy(&u32, bytes.data(), sizeof(uint32_t));

	return u32;
}

ModelLoader::ModelLoader(const std::string& AssetPath, GameObject* Parent)
{
	std::string gltfFilePath = AssetPath;

	m_File = gltfFilePath;

	bool fileExists = true;
	std::string textData = FileRW::ReadFile(gltfFilePath, &fileExists);

	if (!fileExists)
	{
		Debug::Log("Failed to load Model, file '" + AssetPath + "' not found.");
		return;
	}

	// Binary files start with magic number that corresponds to "glTF"
	// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#binary-header
	if (textData.substr(0, 4) == "glTF")
	{
		// skip 12-byte header
		textData = textData.substr(12);

		uint32_t jsonChLength = readU32(textData, 0);
		uint32_t jsonChType = readU32(textData, 4);

		if (jsonChType != 0x4E4F534A)
		{
			Debug::Log(std::vformat(
				"Failed to load Model '{}', first Chunk in binary file was not of type JSON",
				std::make_format_args(AssetPath)
			));
			return;
		}

		std::string jsonString = textData.substr(8, jsonChLength);
		m_JsonData = nlohmann::json::parse(jsonString);

		std::string binaryChunk = textData.substr(8ULL + jsonChLength);

		uint32_t binaryChLength = readU32(binaryChunk, 0);
		uint32_t binaryChType = readU32(binaryChunk, 4);

		if (binaryChType != 0x004E4942)
		{
			Debug::Log(std::vformat(
				"Failed to load Model '{}', second Chunk in binary file was not of type BIN",
				std::make_format_args(AssetPath)
			));
			return;
		}

		std::string binaryString = binaryChunk.substr(8, binaryChLength);

		m_Data = std::vector<uint8_t>(binaryString.begin(), binaryString.end());
	}
	else
	{
		m_JsonData = nlohmann::json::parse(textData);
		m_Data = m_GetData();
	}

	try
	{
		for (const nlohmann::json& scene : m_JsonData["scenes"])
			for (uint32_t node : scene["nodes"])
				m_TraverseNode(node);
	}
	catch (nlohmann::json::type_error e)
	{
		std::string errMessage = e.what();
		throw(std::vformat(
			"Failed to import model '{}': JSON Type Error: {}",
			std::make_format_args(gltfFilePath, errMessage)
		));
	}

	MeshProvider* mp = MeshProvider::Get();

	LoadedObjs.reserve(m_Meshes.size());

	for (size_t MeshIndex = 0; MeshIndex < m_Meshes.size(); MeshIndex++)
	{
		// TODO: cleanup code
		Object_Mesh* mesh = dynamic_cast<Object_Mesh*>(GameObject::Create("Mesh"));
		
		LoadedObjs.push_back(mesh);

		// "meshes/models/crow/0.mesh"
		std::string meshPath = "meshes/"
								+ AssetPath
								+ "/"
								+
								m_MeshNames[MeshIndex]
								+ ".mesh";

		mp->Save(m_Meshes[MeshIndex], meshPath);

		mesh->SetRenderMesh(meshPath);
		mesh->Transform = m_MeshMatrices[MeshIndex];

		//mo->Orientation = this->MeshRotations[MeshIndex];

		mesh->Size = m_MeshScales[MeshIndex];

		TextureManager* texManager = TextureManager::Get();

		nlohmann::json materialJson{};

		ModelLoader::MeshMaterial& material = m_MeshMaterials.at(MeshIndex);

		mesh->ColorRGB = Color(
			material.BaseColorFactor.r,
			material.BaseColorFactor.g,
			material.BaseColorFactor.b
		);
		mesh->Transparency = 1.f - material.BaseColorFactor.a;
		mesh->FaceCulling = material.DoubleSided ? FaceCullingMode::None : FaceCullingMode::BackFace;

		mesh->Name = m_MeshNames[MeshIndex];

		Texture* colorTex = texManager->GetTextureResource(material.BaseColorTexture);
		Texture* metallicRoughnessTex = texManager->GetTextureResource(material.MetallicRoughnessTexture);

		materialJson["albedo"] = colorTex->ImagePath;

		// TODO: glTF assumes proper PBR, with factors from 0 - 1 instead
		// of specular exponents and multipliers etc
		// update this when PBR is added
		// 26/10/2024
		materialJson["specExponent"] = 16.f;
		materialJson["specMultiply"] = material.MetallicFactor;
		materialJson["roughnessFactor"] = material.RoughnessFactor;

		if (material.MetallicRoughnessTexture != 0)
			materialJson["specular"] = metallicRoughnessTex->ImagePath;

		// TODO: Alpha cutoff
		// 29/09/2024
		// `AlphaMode` can be `Opaque`, `Mask` or `Blend`
		// Technically, the Engine only truly supports `Opaque` and `Blend`...
		materialJson["translucency"] = (material.AlphaMode != MeshMaterial::MaterialAlphaMode::Opaque)
			? true
			: false;
		// ... Just in case
		materialJson["alphaMode"] = (int)material.AlphaMode;
		materialJson["alphaCutoff"] = (int)material.AlphaCutoff;

		// `materials/models/crow/feathers.mtl`
		std::string materialName = AssetPath
			+ "/"
			+ material.Name;

		FileRW::WriteFileCreateDirectories(
			"materials/" + materialName + ".mtl",
			materialJson.dump(2),
			true
		);

		mesh->Material = RenderMaterial::GetMaterial(materialName);

		mesh->Transform = m_MeshMatrices[MeshIndex];
		mesh->Size = m_MeshScales[MeshIndex];
		
		//mo->Textures = this->MeshTextures[MeshIndex];

		uint32_t parentMesh = m_MeshParents[MeshIndex];
		if (parentMesh == UINT32_MAX)
			mesh->SetParent(Parent);
		else
			mesh->SetParent(this->LoadedObjs.at(parentMesh));
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
	nlohmann::json& mesh = m_JsonData["meshes"][MeshIndex];
	nlohmann::json& mainPrims = mesh["primitives"][0];

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

	// resize the mesh to 1x1x1 and center it after
	glm::vec3 extMax{};
	glm::vec3 extMin{};

	for (const glm::vec3& position : positions)
	{
		extMax.x = std::max(extMax.x, position.x);
		extMax.y = std::max(extMax.y, position.y);
		extMax.z = std::max(extMax.z, position.z);

		extMin.x = std::min(extMin.x, position.x);
		extMin.y = std::min(extMin.y, position.y);
		extMin.z = std::min(extMin.z, position.z);
	}

	glm::vec3 size = extMax - extMin / 2.f;

	for (size_t index = 0; index < positions.size(); index++)
	{
		positions[index].x /= size.x;
		positions[index].y /= size.y;
		positions[index].z /= size.z;
	}

	glm::vec3 center = positions.at(0);

	for (const glm::vec3& position : positions)
		center += position;

	center /= positions.size();

	for (size_t index = 0; index < positions.size(); index++)
		positions[index] -= center;

	// Combine all the vertex components and also get the indices and textures
	std::vector<Vertex> vertices = m_AssembleVertices(positions, normals, texUVs);
	std::vector<uint32_t> indices = m_GetIndices(m_JsonData["accessors"][indAccInd]);
	
	m_MeshMaterials.push_back(m_GetMaterial(mainPrims));

	size_t meshIndex = m_Meshes.size();

	m_MeshScales[meshIndex] = size;
	m_MeshMatrices[meshIndex] = glm::translate(glm::mat4(1.f), center);

	// Combine the vertices, indices, and textures into a mesh
	m_Meshes.emplace_back(vertices, indices);

	m_MeshNames.push_back(mesh.value(
		"name",
		std::to_string(MeshIndex)
	));
}

void ModelLoader::m_TraverseNode(uint32_t nextNode, glm::mat4 matrix)
{
	// Current node
	nlohmann::json node = m_JsonData["nodes"][nextNode];

	// Get translation if it exists
	glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f);
	if (node.find("translation") != node.end())
	{
		float transValues[3] = 
		{
			node["translation"][0],
			node["translation"][1],
			node["translation"][2]
		};

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
		float scaleValues[3] = 
		{
			node["scale"][0],
			node["scale"][1],
			node["scale"][2]
		};

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
		m_MeshParents.push_back(static_cast<uint32_t>(m_Meshes.size()));

		//m_MeshTranslations.push_back(translation);
		//m_MeshRotations.push_back(rotation);
		m_MeshScales.push_back(scale);
		m_MeshMatrices.push_back(matNextNode);

		m_LoadMesh(node["mesh"]);
	}
	else
		m_MeshParents.push_back(UINT32_MAX);

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

std::vector<float> ModelLoader::m_GetFloats(const nlohmann::json& accessor)
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

std::vector<uint32_t> ModelLoader::m_GetIndices(const nlohmann::json& accessor)
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

ModelLoader::MeshMaterial ModelLoader::m_GetMaterial(const nlohmann::json& Primitive)
{
	TextureManager* texManager = TextureManager::Get();

	ModelLoader::MeshMaterial material;
	material.BaseColorTexture = texManager->LoadTextureFromPath("textures/white.png");

	auto materialIdIt = Primitive.find("material");

	if (materialIdIt == Primitive.end())
		return material;

	int materialId = materialIdIt.value();

	nlohmann::json materialDescription = m_JsonData["materials"][materialId];

	static std::unordered_map<std::string, MeshMaterial::MaterialAlphaMode> NameToAlphaMode =
	{
		{ "OPAQUE", MeshMaterial::MaterialAlphaMode::Opaque },
		{ "MASK", MeshMaterial::MaterialAlphaMode::Mask },
		{ "BLEND", MeshMaterial::MaterialAlphaMode::Blend }
	};

	material.Name = materialDescription.value("name", "PHX_UNNAMED_MATERIAL");
	material.AlphaCutoff = materialDescription.value("alphaCutoff", .5f);
	material.AlphaMode = NameToAlphaMode.find(materialDescription.value("alphaMode", "OPAQUE"))->second;
	material.DoubleSided = materialDescription.value("doubleSided", false);

	nlohmann::json& pbrDescription = materialDescription["pbrMetallicRoughness"];

	nlohmann::json baseColorFactor = pbrDescription.value(
		"baseColorFactor",
		nlohmann::json{ 1.f, 1.f, 1.f, 1.f }
	);

	material.BaseColorFactor = glm::vec4
	{
		baseColorFactor[0],
		baseColorFactor[1],
		baseColorFactor[2],
		baseColorFactor[3]
	};

	material.MetallicFactor = pbrDescription.value("metallicFactor", 1.f);
	material.RoughnessFactor = pbrDescription.value("roughnessFactor", 1.f);

	nlohmann::json baseColDesc = pbrDescription.value(
		"baseColorTexture",
		nlohmann::json{ {"index", 0 } }
	);
	nlohmann::json metallicRoughnessDesc = pbrDescription.value(
		"metallicRoughnessTexture",
		nlohmann::json{ {"index", 0 } }
	);

	nlohmann::json& baseColTex = m_JsonData["textures"][(int)baseColDesc["index"]];
	nlohmann::json& metallicRoughnessTex = m_JsonData["textures"][(int)metallicRoughnessDesc["index"]];

	nlohmann::json& baseColSource = baseColTex["source"];
	int baseColSourceIndex = baseColSource.type() == nlohmann::json::value_t::number_unsigned ?
									(int)baseColSource : -1;

	if (baseColSourceIndex < 0)
		return material;

	std::string baseColPath = m_JsonData["images"][baseColSourceIndex]["uri"];

	std::string fileDirectory = m_File.substr(0, m_File.find_last_of('/') + 1);

	material.BaseColorTexture = texManager->LoadTextureFromPath(fileDirectory + baseColPath);

	if (pbrDescription.find("metallicRoughnessTexture") != pbrDescription.end())
	{
		std::string metallicRoughnessPath = m_JsonData["images"][(int)metallicRoughnessTex["source"]]["uri"];
		material.MetallicRoughnessTexture = texManager->LoadTextureFromPath(fileDirectory + metallicRoughnessPath);
	}

	return material;
}

std::vector<Vertex> ModelLoader::m_AssembleVertices
(
	const std::vector<glm::vec3>& positions,
	const std::vector<glm::vec3>& normals,
	const std::vector<glm::vec2>& texUVs
)
{
	std::vector<Vertex> vertices;
	vertices.reserve(positions.size());

	for (int i = 0; i < positions.size(); i++)
	{
		vertices.emplace_back(
			positions[i],
			normals[i],
			glm::vec3(1.0f, 1.0f, 1.0f),
			texUVs[i]
		);
	}
	return vertices;
}

std::vector<glm::vec2> ModelLoader::m_GroupFloatsVec2(const std::vector<float>& floatVec)
{
	std::vector<glm::vec2> vectors;
	vectors.reserve(static_cast<size_t>(floatVec.size() / 2));

	for (int i = 0; i < floatVec.size(); i)
	{
		vectors.emplace_back(
			floatVec[i++],
			floatVec[i++]
		);
	}
	return vectors;
}

std::vector<glm::vec3> ModelLoader::m_GroupFloatsVec3(const std::vector<float>& floatVec)
{
	std::vector<glm::vec3> vectors;
	vectors.reserve(static_cast<size_t>(floatVec.size() / 3));

	for (int i = 0; i < floatVec.size(); i)
		vectors.emplace_back(
			floatVec[i++],
			floatVec[i++],
			floatVec[i++]
		);

	return vectors;
}
std::vector<glm::vec4> ModelLoader::m_GroupFloatsVec4(const std::vector<float>& floatVec)
{
	std::vector<glm::vec4> vectors;
	vectors.reserve(static_cast<size_t>(floatVec.size() / 4));

	for (int i = 0; i < floatVec.size(); i)
		vectors.emplace_back(
			floatVec[i++],
			floatVec[i++],
			floatVec[i++],
			floatVec[i++]
		);

	return vectors;
}
