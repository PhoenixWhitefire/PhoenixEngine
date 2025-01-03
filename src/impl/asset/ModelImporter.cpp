#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <stb/stb_image.h>

#include "asset/ModelImporter.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/MeshProvider.hpp"
#include "GlobalJsonConfig.hpp"
#include "Utilities.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

static uint32_t readU32(const std::vector<int8_t>& vec, size_t offset)
{
	uint32_t u32{};
	std::memcpy(&u32, vec.data() + offset, 4);

	return u32;
}

static uint32_t readU32(const std::string& str, size_t offset)
{
	std::vector<int8_t> bytes(str.begin() + offset, str.begin() + offset + 4);
	return readU32(bytes, 0);
}

static std::string getTexturePath(
	const std::string& ModelName,
	const nlohmann::json& JsonData,
	const nlohmann::json& ImageJson,
	const std::vector<int8_t>& BufferData
)
{
	if (ImageJson.find("uri") == ImageJson.end())
	{
		std::string mimeType = ImageJson["mimeType"];
		int32_t bufferViewIndex = ImageJson["bufferView"];

		const nlohmann::json& bufferView = JsonData["bufferViews"][bufferViewIndex];

		uint32_t bufferIndex = bufferView["buffer"];
		uint32_t byteOffset = bufferView.value("byteOffset", 0);
		uint32_t byteLength = bufferView["byteLength"];

		if (bufferIndex != 0)
			throw("ModelImporter::getTexturePath got non-zero buffer index " + std::to_string(bufferIndex));

		std::vector<int8_t> imageData(
			BufferData.begin() + byteOffset,
			BufferData.begin() + byteOffset + byteLength
		);

		std::string fileExtension;

		if (mimeType == "image/jpeg")
			fileExtension = ".jpeg";

		else if (mimeType == "image/png")
			fileExtension = ".png";

		else
			fileExtension = ".hxunknownimage";

		std::string filePath = "textures/"
								+ ModelName
								+ "/"
								+ ImageJson.value("name", "UNNAMED")
								+ fileExtension;

		FileRW::WriteFileCreateDirectories(
			filePath,
			imageData,
			true
		);

		return filePath;
	}
	else
	{
		std::string fileDirectory = ModelName.substr(0, ModelName.find_last_of('/') + 1);
		return fileDirectory + (std::string)ImageJson["uri"];
	}
}

ModelLoader::ModelLoader(const std::string& AssetPath, GameObject* Parent)
{
	std::string gltfFilePath = AssetPath;

	m_File = gltfFilePath;

	bool fileExists = true;
	std::string textData = FileRW::ReadFile(gltfFilePath, &fileExists);

	if (!fileExists)
		throw("Failed to load Model, file '" + AssetPath + "' not found.");

	// Binary files start with magic number that corresponds to "glTF"
	// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#binary-header
	if (textData.substr(0, 4) == "glTF")
	{
		uint32_t glbVersion = readU32(textData, 4);
		if (glbVersion != 2)
			Log::Warning(std::vformat(
				"GLB header declares version as '{}', when only `2` is supported. Unexpected behavior may occur.",
				std::make_format_args(glbVersion)
			));

		// header is 12 bytes
		// chunks begin past it
		uint32_t jsonChLength = readU32(textData, 12);
		uint32_t jsonChType = readU32(textData, 16);

		if (jsonChType != 0x4E4F534A)
			throw(std::vformat(
				"Failed to load Model '{}', first Chunk in binary file was not of type JSON",
				std::make_format_args(AssetPath)
			));

		std::string jsonString = textData.substr(20, jsonChLength);
		m_JsonData = nlohmann::json::parse(jsonString);

		// 20ull because
		// 12 byte header + 4 byte chunk length + 4 byte chunk type
		std::vector<int8_t> binaryChunk(
			textData.begin() + 20ull + jsonChLength,
			textData.end()
		);

		uint32_t binaryChLength = readU32(binaryChunk, 0);
		uint32_t binaryChType = readU32(binaryChunk, 4);

		if (binaryChType != 0x004E4942)
			throw(std::vformat(
				"Failed to load Model '{}', second Chunk in binary file was not of type BIN",
				std::make_format_args(AssetPath)
			));

		// + 8 because 8 byte header
		m_Data = std::vector<int8_t>(binaryChunk.begin() + 8, binaryChunk.begin() + binaryChLength + 8);
	}
	else
	{
		try
		{
			m_JsonData = nlohmann::json::parse(textData);
		}
		PHX_CATCH_AND_RETHROW(
			nlohmann::json::parse_error,
			std::string("Failed to import model (parse error): ") + gltfFilePath +,
			.what()
		);

		m_Data = m_GetData();
	}

	try
	{
		const nlohmann::json& assetInfoJson = m_JsonData["asset"];

		if (assetInfoJson.find("minVersion") != assetInfoJson.end())
		{
			float gltfMinVersion = std::stof(assetInfoJson.value("minVersion", "2.0"));

			Log::Warning(std::vformat(
				"glTF file specifies `asset.minVersion` as '{}'. Unexpected behavior may occur.",
				std::make_format_args(gltfMinVersion)
			));
		}
		else
		{
			float gltfVersion = std::stof(assetInfoJson.value("version", "2.0"));

			if (gltfVersion < 2.f || gltfVersion >= 3.f)
				Log::Warning(std::vformat(
					"Expected glTF version >= 2.0 and < 3.0, got {}. Unexpected behavior may occur.",
					std::make_format_args(gltfVersion)
				));
		}

		const nlohmann::json& requiredExtensionsJson = m_JsonData["extensionsRequired"];
		const nlohmann::json& usedExtensionsJson = m_JsonData["extensionsUsed"];

		for (std::string v : requiredExtensionsJson)
			Log::Warning(std::vformat(
				"glTF file specifies 'required' extension '{}'. That's too bad, because no extensions are supported.",
				std::make_format_args(v)
			));

		for (std::string v : usedExtensionsJson)
			Log::Warning(std::vformat(
				"glTF file specifies extension '{}' is used. That's too bad, because no extensions are supported.",
				std::make_format_args(v)
			));

		// actually load the model

		m_Nodes.emplace_back(
			AssetPath,
			UINT32_MAX,
			true
		);

		for (const nlohmann::json& scene : m_JsonData["scenes"])
			// root nodes
			for (uint32_t node : scene["nodes"])
				m_TraverseNode(node, 0);
	}
	PHX_CATCH_AND_RETHROW(
		nlohmann::json::type_error,
		std::string("Failed to import model (type error): ") + gltfFilePath +,
		.what()
	);

	MaterialManager* mtlManager = MaterialManager::Get();
	MeshProvider* meshProvider = MeshProvider::Get();

	LoadedObjs.reserve(m_Nodes.size() + 1);

	for (const ModelLoader::ModelNode& node : m_Nodes)
	{
		GameObject* object{};

		if (node.IsContainerOnlyWithoutGeo)
			object = GameObject::Create("Model");
		else
		{
			// TODO: cleanup code
			object = GameObject::Create("Mesh");
			Object_Mesh* meshObject = static_cast<Object_Mesh*>(object);

			/*
				When;
					AssetPath = "models/crow/scene.gltf"
					MeshName = "main.001"

				Then:
					meshPath = "meshes/models/crow/scene.gltf/main.001.hxmesh"

				Could be cleaner, but it doesn't matter
				22/12/2024
			*/
			std::string meshPath = "meshes/"
				+ AssetPath
				+ "/"
				+ node.Name
				+ ".hxmesh";

			meshProvider->Save(node.Data, meshPath);

			meshObject->SetRenderMesh(meshPath);
			meshObject->Transform = node.Transform;

			//mo->Orientation = this->MeshRotations[MeshIndex];

			meshObject->Size = node.Scale;

			TextureManager* texManager = TextureManager::Get();

			nlohmann::json materialJson{};

			const ModelLoader::MeshMaterial& material = node.Material;

			meshObject->Tint = Color(
				material.BaseColorFactor.r,
				material.BaseColorFactor.g,
				material.BaseColorFactor.b
			);
			meshObject->Transparency = 1.f - material.BaseColorFactor.a;
			meshObject->FaceCulling = material.DoubleSided ? FaceCullingMode::None : FaceCullingMode::BackFace;

			Texture& colorTex = texManager->GetTextureResource(material.BaseColorTexture);
			Texture& metallicRoughnessTex = texManager->GetTextureResource(material.MetallicRoughnessTexture);
			Texture& normalTex = texManager->GetTextureResource(material.NormalTexture);
			Texture& emissiveTex = texManager->GetTextureResource(material.EmissiveTexture);

			materialJson["ColorMap"] = colorTex.ImagePath;

			// TODO: glTF assumes proper PBR, with factors from 0 - 1 instead
			// of specular exponents and multipliers etc
			// update this when PBR is added
			// 26/10/2024
			materialJson["specExponent"] = 16.f;

			if (material.MetallicRoughnessTexture != 0)
				materialJson["MetallicRoughnessMap"] = metallicRoughnessTex.ImagePath;

			if (material.NormalTexture != 0)
				materialJson["NormalMap"] = normalTex.ImagePath;

			if (material.EmissiveTexture != 0)
				materialJson["EmissionMap"] = emissiveTex.ImagePath;

			materialJson["HasTranslucency"] = (material.AlphaMode == MeshMaterial::MaterialAlphaMode::Blend);

			materialJson["Uniforms"] = nlohmann::json::object();

			if (material.AlphaMode == MeshMaterial::MaterialAlphaMode::Mask)
				materialJson["Uniforms"]["AlphaCutoff"] = material.AlphaCutoff;

			materialJson["BilinearFiltering"] = colorTex.DoBilinearSmoothing;

			// `models/crow/feathers`
			// `models/EmbeddedTexture.glb/Material.001`
			std::string materialName = AssetPath
				+ "/"
				+ material.Name;

			FileRW::WriteFileCreateDirectories(
				"materials/" + materialName + ".mtl",
				materialJson.dump(2),
				true
			);

			meshObject->MaterialId = mtlManager->LoadMaterialFromPath(materialName);
			mtlManager->SaveToPath(mtlManager->GetMaterialResource(meshObject->MaterialId), materialName);

			meshObject->MetallnessFactor = material.MetallicFactor;
			meshObject->RoughnessFactor = material.RoughnessFactor;
		}

		object->Name = node.Name;

		LoadedObjs.push_back(object);

		//mo->Textures = this->MeshTextures[MeshIndex];

		uint32_t parentIndex = node.Parent;
		if (parentIndex == UINT32_MAX) // root node
			object->SetParent(Parent);
		else
			object->SetParent(this->LoadedObjs.at(parentIndex) != object ? LoadedObjs[parentIndex] : Parent);
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

ModelLoader::ModelNode ModelLoader::m_LoadPrimitive(
	const nlohmann::json& MeshData,
	uint32_t PrimitiveIndex,
	const glm::mat4& Transform,
	const glm::vec3& Scale
)
{
	const nlohmann::json& primitive = MeshData["primitives"][PrimitiveIndex];

	// Get all accessor indices
	uint32_t posAccInd = primitive["attributes"]["POSITION"];
	uint32_t normalAccInd = primitive["attributes"]["NORMAL"];
	uint32_t texAccInd = primitive["attributes"]["TEXCOORD_0"];
	uint32_t indAccInd = primitive["indices"];

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

	center /= static_cast<float>(positions.size());

	for (size_t index = 0; index < positions.size(); index++)
		positions[index] -= center;

	// Combine all the vertex components and also get the indices and textures
	std::vector<Vertex> vertices = m_AssembleVertices(positions, normals, texUVs);
	std::vector<uint32_t> indices = m_GetIndices(m_JsonData["accessors"][indAccInd]);
	
	return
	{
		// e.g. "Cube", "Cube2" on 2nd prim, "_UNNAMED-0_" w/o name and 1st prim
		MeshData.value(
			"name",
			"_UNNAMED-" + std::to_string(PrimitiveIndex) + "_"
		) + (PrimitiveIndex > 0 ? std::to_string(PrimitiveIndex + 1) : ""),
		0u,
		false,

		Mesh{ vertices, indices },
		m_GetMaterial(primitive),
		Transform * glm::translate(glm::mat4(1.f), center),
		Scale * size
	};
}

void ModelLoader::m_TraverseNode(uint32_t NodeIndex, uint32_t From, const glm::mat4& Transform)
{
	// Current node
	const nlohmann::json& nodeJson = m_JsonData["nodes"][NodeIndex];

	// Get translation if it exists
	glm::vec3 translation{};
	if (const auto transIt = nodeJson.find("translation"); transIt != nodeJson.end())
	{
		const nlohmann::json& trans = transIt.value();

		float transValues[3] = 
		{
			trans[0],
			trans[1],
			trans[2]
		};

		translation = glm::make_vec3(transValues);
	}
	// Get quaternion if it exists
	glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
	if (const auto rotIt = nodeJson.find("rotation"); rotIt != nodeJson.end())
	{
		const nlohmann::json& rot = rotIt.value();

		float rotValues[4] =
		{
			rot[3],
			rot[0],
			rot[1],
			rot[2]
		};
		rotation = glm::make_quat(rotValues);
	}
	// Get scale if it exists
	glm::vec3 scale{ 1.f, 1.f, 1.f };
	if (const auto scaleIt = nodeJson.find("scale"); scaleIt != nodeJson.end())
	{
		const nlohmann::json& sca = scaleIt.value();

		float scaleValues[3] = 
		{
			sca[0],
			sca[1],
			sca[2]
		};

		scale = glm::make_vec3(scaleValues);
	}
	// Get matrix if it exists
	glm::mat4 matNode = glm::mat4(1.f);
	if (const auto matIt = nodeJson.find("matrix"); matIt != nodeJson.end())
	{
		const nlohmann::json& mat = matIt.value();

		float matValues[16]{};
		for (uint32_t i = 0; i < std::min(mat.size(), 16ull); i++)
			matValues[i] = mat[i];

		matNode = glm::make_mat4(matValues);
	}

	glm::mat4 trans = glm::translate(glm::mat4(1.f), translation); // 14/09/2024 har har har
	glm::mat4 rot = glm::mat4_cast(rotation);

	// Multiply all matrices together
	glm::mat4 matNextNode = Transform * matNode * trans * rot;

	uint32_t myIndex = static_cast<uint32_t>(m_Nodes.size());

	// Check if the node contains a mesh and if it does load it
	if (const auto meshDataIt = nodeJson.find("mesh"); meshDataIt != nodeJson.end())
	{
		uint32_t meshId = meshDataIt.value();

		const nlohmann::json& meshData = m_JsonData["meshes"][meshId];

		//m_MeshTranslations.push_back(translation);
		//m_MeshRotations.push_back(rotation);

		// 30/12/2024
		// https://math.stackexchange.com/a/1463487
		glm::vec3 embeddedScale
		{
			glm::length(glm::vec3(matNextNode[0][0], matNextNode[1][0], matNextNode[2][0])),
			glm::length(glm::vec3(matNextNode[0][1], matNextNode[1][1], matNextNode[2][1])),
			glm::length(glm::vec3(matNextNode[0][2], matNextNode[1][2], matNextNode[2][2]))
		};

		scale *= embeddedScale;

		glm::mat4 thisNodeTransform = matNextNode * glm::scale(glm::mat4(1.f), 1.f / embeddedScale);

		for (uint32_t i = 0; i < meshData["primitives"].size(); i++)
		{
			ModelNode node = m_LoadPrimitive(meshData, i, thisNodeTransform, scale);
			node.Parent = From;

			m_Nodes.push_back(node);
		}
	}
	else
		m_Nodes.emplace_back(
			nodeJson.value("name", "_UNNAMED_CONTAINER-" + std::to_string(NodeIndex) + "_"),
			From,
			true
		);

	// Check if the node has children, and if it does, apply this function to them with the matNextNode
	if (const auto chIt = nodeJson.find("children"); chIt != nodeJson.end())
	{
		const nlohmann::json& children = chIt.value();

		if (children.size() == 1)
			// there's no point having a container that houses just 1 node
			// 31/12/2024
		{
			m_Nodes.erase(m_Nodes.end() - 1);
			m_TraverseNode(children[0], From, matNextNode);
		}
		else
			for (const nlohmann::json& subnode : children)
				m_TraverseNode(subnode, myIndex, matNextNode);
	}
}

std::vector<int8_t> ModelLoader::m_GetData()
{
	// Create a place to store the raw text, and get the uri of the .bin file
	std::string bytesText;
	std::string uri = m_JsonData["buffers"][0]["uri"];

	// Store raw text data into bytesText
	std::string fileStr = std::string(m_File);
	std::string fileDirectory = fileStr.substr(0, fileStr.find_last_of('/') + 1);
	bytesText = FileRW::ReadFile((fileDirectory + uri));

	// Transform the raw text data into bytes and put them in a vector
	std::vector<int8_t> data(bytesText.begin(), bytesText.end());
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
	const nlohmann::json& bufferView = m_JsonData["bufferViews"][buffViewInd];
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
		int8_t bytes[] = { m_Data[i++], m_Data[i++], m_Data[i++], m_Data[i++] };
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
	const nlohmann::json& bufferView = m_JsonData["bufferViews"][buffViewInd];
	uint32_t byteOffset = bufferView.value("byteOffset", 0);

	// Get indices with regards to their type: uint32_t, uint16_t, or short
	uint32_t beginningOfData = byteOffset + accByteOffset;
	if (componentType == 5125)
	{
		for (uint32_t i = beginningOfData; i < byteOffset + accByteOffset + count * 4; i)
		{
			int8_t bytes[] = { m_Data[i++], m_Data[i++], m_Data[i++], m_Data[i++] };
			uint32_t value;
			std::memcpy(&value, bytes, sizeof(uint32_t));
			indices.push_back(value);
		}
	}
	else if (componentType == 5123)
	{
		for (uint32_t i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
		{
			int8_t bytes[] = { m_Data[i++], m_Data[i++] };
			uint16_t value;
			std::memcpy(&value, bytes, sizeof(uint16_t));
			indices.push_back(value);
		}
	}
	else if (componentType == 5122)
	{
		for (uint32_t i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
		{
			int8_t bytes[] = { m_Data[i++], m_Data[i++] };
			short value;
			std::memcpy(&value, bytes, sizeof(short));
			indices.push_back(value);
		}
	}
	else
		Log::Warning("Unrecognized mesh index type: " + std::to_string(componentType));

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

	const nlohmann::json& materialDescription = m_JsonData["materials"][materialId];

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

	const nlohmann::json& pbrDescription = materialDescription["pbrMetallicRoughness"];

	const nlohmann::json& baseColorFactor = pbrDescription.value(
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

	const nlohmann::json& baseColDesc = pbrDescription.value(
		"baseColorTexture",
		nlohmann::json{ { "index", 0 } }
	);
	const nlohmann::json& metallicRoughnessDesc = pbrDescription.value(
		"metallicRoughnessTexture",
		nlohmann::json{ { "index", 0 } }
	);
	const nlohmann::json& normalDesc = materialDescription.value(
		"normalTexture",
		nlohmann::json{ { "index", 0 } }
	);
	const nlohmann::json& emissiveDesc = materialDescription.value(
		"emissiveTexture",
		nlohmann::json{ { "index", 0 } }
	);

	nlohmann::json& baseColTex = m_JsonData["textures"][(int)baseColDesc["index"]];
	nlohmann::json& metallicRoughnessTex = m_JsonData["textures"][(int)metallicRoughnessDesc["index"]];
	nlohmann::json& normalTex = m_JsonData["textures"][(int)normalDesc["index"]];
	nlohmann::json& emissiveTex = m_JsonData["textures"][(int)emissiveDesc["index"]];

	nlohmann::json& baseColSource = baseColTex["source"];
	int baseColSourceIndex = baseColSource.type() == nlohmann::json::value_t::number_unsigned ?
									(int)baseColSource : -1;

	if (baseColSourceIndex < 0)
		return material;

	bool baseColFilterBilinear = true;

	if (baseColTex.find("sampler") != baseColTex.end())
		// 9729 == LINEAR 
		// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#schema-reference-sampler
		baseColFilterBilinear = m_JsonData["samplers"][(int)baseColTex["sampler"]]["magFilter"] == 9729;
	
	std::string baseColPath = getTexturePath(
		m_File,
		m_JsonData,
		m_JsonData["images"][baseColSourceIndex],
		m_Data
	);

	material.BaseColorTexture = texManager->LoadTextureFromPath(
		baseColPath,
		true,
		baseColFilterBilinear
	);

	texManager->GetTextureResource(material.BaseColorTexture).DoBilinearSmoothing = baseColFilterBilinear;

	if (pbrDescription.find("metallicRoughnessTexture") != pbrDescription.end())
	{
		std::string metallicRoughnessPath = getTexturePath(
			m_File,
			m_JsonData,
			m_JsonData["images"][(int)metallicRoughnessTex["source"]],
			m_Data
		);

		material.MetallicRoughnessTexture = texManager->LoadTextureFromPath(
			metallicRoughnessPath,
			true,
			baseColFilterBilinear
		);
	}

	if (materialDescription.find("normalTexture") != materialDescription.end())
	{
		std::string normalPath = getTexturePath(
			m_File,
			m_JsonData,
			m_JsonData["images"][(int)normalTex["source"]],
			m_Data
		);

		material.NormalTexture = texManager->LoadTextureFromPath(
			normalPath,
			true,
			baseColFilterBilinear
		);
	}

	if (materialDescription.find("emissiveTexture") != materialDescription.end())
	{
		std::string emissivePath = getTexturePath(
			m_File,
			m_JsonData,
			m_JsonData["images"][(int)emissiveTex["source"]],
			m_Data
		);

		material.EmissiveTexture = texManager->LoadTextureFromPath(
			emissivePath,
			true,
			baseColFilterBilinear
		);
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
			glm::vec4(1.f, 1.f, 1.f, 1.f),
			texUVs[i]
		);
	}
	return vertices;
}

std::vector<glm::vec2> ModelLoader::m_GroupFloatsVec2(const std::vector<float>& floatVec)
{
	std::vector<glm::vec2> vectors;
	vectors.reserve(static_cast<size_t>(floatVec.size() / 2));

	for (int i = 0; i < floatVec.size(); i += 2)
		vectors.emplace_back(
			floatVec[i+0ull],
			floatVec[i+1ull]
		);

	return vectors;
}

std::vector<glm::vec3> ModelLoader::m_GroupFloatsVec3(const std::vector<float>& floatVec)
{
	std::vector<glm::vec3> vectors;
	vectors.reserve(static_cast<size_t>(floatVec.size() / 3));

	for (int i = 0; i < floatVec.size(); i += 3)
		vectors.emplace_back(
			floatVec[i+2ull],
			floatVec[i+1ull],
			floatVec[i+0ull]
		);

	return vectors;
}
std::vector<glm::vec4> ModelLoader::m_GroupFloatsVec4(const std::vector<float>& floatVec)
{
	std::vector<glm::vec4> vectors;
	vectors.reserve(static_cast<size_t>(floatVec.size() / 4));

	for (int i = 0; i < floatVec.size(); i += 4)
		vectors.emplace_back(
			floatVec[i+3ull],
			floatVec[i+2ull],
			floatVec[i+1ull],
			floatVec[i+0ull]
		);

	return vectors;
}
