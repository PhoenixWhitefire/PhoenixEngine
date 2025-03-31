#include <filesystem>
#include <cfloat>
#include <glm/gtc/type_ptr.hpp>
#include <stb/stb_image.h>
#include <tracy/Tracy.hpp>

#include "asset/ModelImporter.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/MeshProvider.hpp"
#include "component/Transformable.hpp"
#include "component/Mesh.hpp"
#include "component/Bone.hpp"
#include "GlobalJsonConfig.hpp"
#include "Utilities.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

static uint32_t readU32(const std::string_view& vec, size_t offset)
{
	return *(uint32_t*)&vec.at(offset);
}

static std::string getTexturePath(
	const std::string& ModelName,
	const nlohmann::json& JsonData,
	const nlohmann::json& ImageJson,
	const std::string_view& BufferData
)
{
	if (ImageJson.find("uri") == ImageJson.end())
	{
		ZoneScopedN("ExtractImageData");

		std::string mimeType = ImageJson["mimeType"];
		int32_t bufferViewIndex = ImageJson["bufferView"];

		const nlohmann::json& bufferView = JsonData["bufferViews"][bufferViewIndex];

		uint32_t bufferIndex = bufferView["buffer"];
		uint32_t byteOffset = bufferView.value("byteOffset", 0);
		uint32_t byteLength = bufferView["byteLength"];

		if (bufferIndex != 0)
			throw("ModelImporter::getTexturePath got non-zero buffer index " + std::to_string(bufferIndex));

		std::string_view imageData{ BufferData.begin() + byteOffset, BufferData.begin() + byteOffset + byteLength };

		std::string fileExtension;

		if (mimeType == "image/jpeg")
			fileExtension = ".jpeg";

		else if (mimeType == "image/png")
			fileExtension = ".png";

		else
		{
			Log::Warning(std::vformat("Unrecognized MIME '{}'", std::make_format_args(mimeType)));
			fileExtension = std::string(mimeType.begin() + 6, mimeType.end());
		}

		std::string filePath = "textures/"
								+ ModelName
								+ "/"
								+ ImageJson.value("name", "UNNAMED")
								+ fileExtension;

		bool writeSucceeded = true;

		FileRW::WriteFileCreateDirectories(
			filePath,
			imageData,
			true,
			&writeSucceeded
		);

		if (!writeSucceeded)
			Log::Warning(std::vformat(
				"Failed to extract image from Model '{}' to path: {}",
				std::make_format_args(ModelName, filePath)
			));

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
	ZoneScoped;

	std::string gltfFilePath = FileRW::TryMakePathCwdRelative(AssetPath);

	m_File = gltfFilePath;

	bool fileExists = true;
	std::string textData = FileRW::ReadFile(gltfFilePath, &fileExists, false);

	if (!fileExists)
		throw("Failed to load Model, file '" + AssetPath + "' not found.");

	// Binary files start with magic number that corresponds to "glTF"
	// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#binary-header
	if (std::string_view(textData.begin(), textData.begin() + 4) == "glTF")
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

		ZoneScopedN("ParseGLBStructure");

		std::string_view jsonString{ textData.begin() + 20, textData.begin() + 20 + jsonChLength };
		m_JsonData = nlohmann::json::parse(jsonString);

		// 20ull because
		// 12 byte header + 4 byte chunk length + 4 byte chunk type
		std::string_view binaryChunk{ textData.begin() + 20ull + jsonChLength, textData.end() };

		uint32_t binaryChLength = readU32(binaryChunk, 0);
		uint32_t binaryChType = readU32(binaryChunk, 4);

		if (binaryChType != 0x004E4942)
			throw(std::vformat(
				"Failed to load Model '{}', second Chunk in binary file was not of type BIN",
				std::make_format_args(AssetPath)
			));

		// + 8 because 8 byte header
		m_Data = std::string(binaryChunk.begin() + 8, binaryChunk.begin() + 8 + binaryChLength);
	}
	else
	{
		try
		{
			ZoneScopedN("ParseGLTF");
			m_JsonData = nlohmann::json::parse(textData);
		}
		catch (nlohmann::json::parse_error Error)
		{
			throw("Failed to import model due to parse error: " + std::string(Error.what()));
		}

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
			UINT32_MAX,
			ModelNode::NodeType::Container
		);

		ZoneScopedN("ParseNodes");

		for (const nlohmann::json& scene : m_JsonData["scenes"])
			// root nodes
			for (uint32_t node : scene["nodes"])
				m_TraverseNode(node, 0);

		m_BuildRig();
	}
	catch (nlohmann::json::type_error Error)
	{
		throw("Failed to import model due to type error: " + std::string(Error.what()));
	}

	ZoneName("ImportNodes", 11);

	MaterialManager* mtlManager = MaterialManager::Get();
	MeshProvider* meshProvider = MeshProvider::Get();

	LoadedObjs.reserve(m_Nodes.size() + 1);

	for (ModelLoader::ModelNode& node : m_Nodes)
	{
		GameObject* object{};

		if (node.Type == ModelNode::NodeType::Container)
			object = GameObject::Create("Model");

		else if (node.Type == ModelNode::NodeType::Primitive)
		{
			// TODO: cleanup code
			object = GameObject::Create("Mesh");
			EcMesh* meshObject = object->GetComponent<EcMesh>();
			EcTransformable* ct = object->GetComponent<EcTransformable>();

			std::string saveDir = AssetPath;
			size_t whereRes = AssetPath.find("resources/");

			if (whereRes == std::string::npos)
				Log::Warning(std::vformat(
					"ModelLoader cannot guarantee the mesh will be saved within the Resources directory (Path was: '{}')",
					std::make_format_args(AssetPath)
				));
			else
				saveDir = saveDir.substr(whereRes + 10, saveDir.size() - whereRes);

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
									+ saveDir
									+ "/"
									+ node.Name
									+ ".hxmesh";

			meshProvider->Save(node.Data, meshPath);
			meshProvider->Assign(node.Data, meshPath);

			meshObject->SetRenderMesh(meshPath);
			ct->Transform = node.Transform;

			//mo->Orientation = this->MeshRotations[MeshIndex];

			ct->Size = node.Scale;

			meshObject->RecomputeAabb();

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

			// `models/EmbeddedTexture.glb/Material.001`
			std::string materialName = saveDir
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
		else
		{
			object = GameObject::Create("Mesh");
			EcTransformable* prim = object->GetComponent<EcTransformable>();
			prim->Transform = node.Transform;
			prim->Size = node.Scale;
		}

		object->Name = node.Name;

		LoadedObjs.push_back(object);

		uint32_t parentIndex = node.Parent;
		if (parentIndex == UINT32_MAX) // root node
			object->SetParent(Parent);
		else
			object->SetParent(this->LoadedObjs.at(parentIndex) != object ? LoadedObjs[parentIndex] : Parent);
	}

	for (GameObject* anim : m_Animations)
		anim->SetParent(LoadedObjs.at(0));
}

ModelLoader::ModelNode ModelLoader::m_LoadPrimitive(
	const nlohmann::json& MeshData,
	uint32_t PrimitiveIndex,
	const glm::mat4& Transform,
	const glm::vec3& Scale
)
{
	ZoneScoped;

	const nlohmann::json& primitive = MeshData["primitives"][PrimitiveIndex];
	const nlohmann::json& attributes = primitive["attributes"];
	const nlohmann::json& accessors = m_JsonData["accessors"];

	// Get all accessor indices
	uint32_t posAccInd = attributes["POSITION"];
	uint32_t normalAccInd = attributes["NORMAL"];
	uint32_t texAccInd = attributes["TEXCOORD_0"];
	uint32_t indAccInd = primitive["indices"];

	auto colAccIt = attributes.find("COLOR_0");
	auto jointsAccIt = attributes.find("JOINTS_0");
	auto weightsAccIt = attributes.find("WEIGHTS_0");

	// Use accessor indices to get all vertices components
	std::vector<glm::vec3> positions = m_GetAndGroupFloatsVec3(accessors[posAccInd]);
	std::vector<glm::vec3> normals = m_GetAndGroupFloatsVec3(accessors[normalAccInd]);
	std::vector<glm::vec2> texUVs = m_GetAndGroupFloatsVec2(accessors[texAccInd]);

	std::vector<glm::vec4> cols{};
	std::vector<glm::tvec4<uint8_t>> joints{};
	std::vector<glm::vec4> weights{};

	if (colAccIt != attributes.end())
		cols = m_GetAndGroupFloatsVec4(accessors[(uint32_t)colAccIt.value()]);
	else
		for (size_t i = 0; i < positions.size(); i++)
			cols.emplace_back(1.f, 1.f, 1.f, 1.f);

	if (jointsAccIt != attributes.end() && weightsAccIt != attributes.end())
	{
		uint32_t jointsAcc = jointsAccIt.value();
		uint32_t weightsAcc = weightsAccIt.value();

		joints = m_GetAndGroupUBytesVec4(accessors[jointsAcc]);
		weights = m_GetAndGroupFloatsVec4(accessors[weightsAcc]);
	}

	// normalize and center the mesh 11/01/2024
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

	glm::vec3 size = (extMax - extMin);
	glm::vec3 center = (extMin + extMax) * .5f;

	glm::vec3 sizeInv = glm::vec3(
		size.x > 0.f ? (1.f/size.x) : 1.f,
		size.y > 0.f ? (1.f/size.y) : 1.f,
		size.z > 0.f ? (1.f/size.z) : 1.f
	);

	//https://stackoverflow.com/a/69808766
	glm::mat4 matS = glm::scale(glm::mat4(1.f), sizeInv);
	glm::mat4 matT = glm::translate(glm::mat4(1.f), -center);
	glm::mat4 matM = matS * matT;

	for (glm::vec3& position : positions)
		position = glm::vec3(matM * glm::vec4(position, 1.f));

	// Combine all the vertex components and also get the indices and textures
	std::vector<Vertex> vertices = m_AssembleVertices(positions, normals, texUVs, cols, joints, weights);
	std::vector<uint32_t> indices = m_GetUnsigned32s(accessors[indAccInd]);
	
	return
	{
		// e.g. "Cube", "Cube2" on 2nd prim, "_UNNAMED-0_" w/o name and 1st prim
		MeshData.value(
			"name",
			"_UNNAMED-" + std::to_string(PrimitiveIndex) + "_"
		) + (PrimitiveIndex > 0 ? std::to_string(PrimitiveIndex + 1) : ""),
		UINT32_MAX,
		0u,
		ModelNode::NodeType::Primitive,

		Mesh{ vertices, indices },
		m_GetMaterial(primitive),
		Transform * glm::translate(glm::mat4(1.f), center),
		Scale * size
	};
}

void ModelLoader::m_TraverseNode(uint32_t NodeIndex, uint32_t From, const glm::mat4& Transform)
{
	ZoneScoped;

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
		for (uint32_t i = 0; i < std::min(mat.size(), (size_t)16); i++)
			matValues[i] = mat[i];

		matNode = glm::make_mat4(matValues);
	}

	glm::mat4 trans = glm::translate(glm::mat4(1.f), translation); // 14/09/2024 har har har
	glm::mat4 rot = glm::mat4_cast(rotation);

	// Multiply all matrices together
	glm::mat4 matNextNode = Transform * matNode * trans * rot;

	uint32_t myIndex = static_cast<uint32_t>(m_Nodes.size());
	m_NodeIdToIndex[NodeIndex] = myIndex;

	// Check if the node contains a mesh and if it does load it
	if (const auto meshDataIt = nodeJson.find("mesh"); meshDataIt != nodeJson.end())
	{
		uint32_t meshId = meshDataIt.value();

		const nlohmann::json& meshData = m_JsonData["meshes"][meshId];

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

		nlohmann::json skinJson{};
		bool isSkinned = false;

		if (const auto skinIt = nodeJson.find("skin"); skinIt != nodeJson.end())
		{
			skinJson = m_JsonData["skins"][(int32_t)skinIt.value()];
			isSkinned = true;
		}

		for (uint32_t i = 0; i < meshData["primitives"].size(); i++)
		{
			ModelNode node = m_LoadPrimitive(meshData, i, thisNodeTransform, scale);
			node.NodeId = NodeIndex;
			node.Parent = From;

			if (isSkinned)
			{
				const nlohmann::json& jointsJson = skinJson["joints"];

				std::vector<glm::mat4> invBindMatrices;
				invBindMatrices.resize(jointsJson.size(), 1.f);

				if (const auto invBindMtx = skinJson.find("inverseBindMatrices"); invBindMtx != skinJson.end())
				{
					const nlohmann::json& accessor = m_JsonData["accessors"][(int32_t)invBindMtx.value()];
					invBindMatrices = m_GetAndGroupFloatsMat4(accessor);
				}

				for (int32_t jointNodeIdx = 0; jointNodeIdx < jointsJson.size(); jointNodeIdx++)
					node.Bones.emplace_back((int32_t)skinJson["joints"][jointNodeIdx], invBindMatrices[jointNodeIdx]);
			}

			m_Nodes.push_back(node);
		}
	}
	else
		m_Nodes.emplace_back(
			nodeJson.value("name", "_UNNAMED_CONTAINER-" + std::to_string(NodeIndex) + "_"),
			NodeIndex,
			From,
			ModelNode::NodeType::Container
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

void ModelLoader::m_BuildRig()
{
	ZoneScoped;

	for (ModelNode& node : m_Nodes)
		for (const ModelNode::BoneInfo& jointDesc : node.Bones)
		{
			uint32_t jointNodeIndex = m_NodeIdToIndex.at(jointDesc.NodeId);
			const ModelNode& jointNode = m_Nodes.at(jointNodeIndex);

			node.Data.Bones.emplace_back(
				jointNode.Name,
				jointNode.Transform,
				jointNode.Scale,
				jointDesc.InvBindMatrix
			);
		}

	for (const nlohmann::json& animationJson : m_JsonData.value("animations", nlohmann::json::array()))
	{
		GameObject* anim = GameObject::Create("Animation");
		anim->Name = animationJson["name"];

		for (const nlohmann::json& channelJson : animationJson["channels"])
		{
			uint32_t targetId = m_NodeIdToIndex[channelJson["target"]["node"]];
			ModelNode& target = m_Nodes[targetId];

			target.Type = ModelNode::NodeType::Bone;
		}

		m_Animations.push_back(anim);
	}
}

std::string ModelLoader::m_GetData()
{
	// Create a place to store the raw text, and get the uri of the .bin file
	std::string bytesText;
	std::string uri = m_JsonData["buffers"][0]["uri"];

	// Store raw text data into bytesText
	std::string fileDirectory = m_File.substr(0, m_File.find_last_of('/') + 1);

	return FileRW::ReadFile(fileDirectory + uri);
}

std::vector<float> ModelLoader::m_GetFloats(const nlohmann::json& accessor)
{
	ZoneScoped;

	std::vector<float> floatVec;

	// Get properties from the accessor
	uint32_t buffViewInd = accessor.value("bufferView", 1);
	uint32_t count = accessor["count"];
	uint32_t accByteOffset = accessor.value("byteOffset", 0);
	std::string type = accessor["type"];
	uint32_t componentType = accessor["componentType"];

	// Get properties from the bufferView
	const nlohmann::json& bufferView = m_JsonData["bufferViews"][buffViewInd];
	uint32_t byteOffset = bufferView["byteOffset"];

	// Interpret the type and store it into numPerVert
	uint32_t numPerVert{};
	if (type == "SCALAR")
		numPerVert = 1;

	else if (type == "VEC2")
		numPerVert = 2;

	else if (type == "VEC3")
		numPerVert = 3;

	else if (type == "VEC4")
		numPerVert = 4;

	else if (type == "MAT4")
		numPerVert = 16;

	else
		throw("Could not decode GLTF model: Type is not handled (not SCALAR, VEC2, VEC3, or VEC4)");

	uint32_t componentSize{};
	switch (componentType)
	{
	case 5123:
	{
		componentSize = 2;
		break;
	}
	case 5126:
	{
		componentSize = 4;
		break;
	}
	default:
		throw("Unsupported `componentType` of " + std::to_string(componentType));
	}

	// Go over all the bytes in the data at the correct place using the properties from above
	uint32_t beginningOfData = byteOffset + accByteOffset;
	uint32_t lengthOfData = count * componentSize * numPerVert;
	for (uint32_t i = beginningOfData; i < beginningOfData + lengthOfData; i)
	{
		if (componentType == 5126)
		{
			float value = *(float*)&m_Data[i++];
			floatVec.push_back(value);

			i += 3;
		}
		else if (componentType == 5123)
		{
			uint16_t us = *(uint16_t*)&m_Data[i++];
			floatVec.push_back(us / 65535.f);

			i += 1;
		}
		else
			throw("huh??");
	}

	return floatVec;
}

std::vector<uint32_t> ModelLoader::m_GetUnsigned32s(const nlohmann::json& accessor)
{
	ZoneScoped;

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
			uint32_t value = *(uint32_t*)&m_Data[i++];
			indices.push_back(value);

			i += 3;
		}
	}
	else if (componentType == 5123)
	{
		for (uint32_t i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
		{
			uint16_t value = *(uint16_t*)&m_Data[i++];
			indices.push_back(value);

			i += 1;
		}
	}
	else if (componentType == 5122)
	{
		for (uint32_t i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
		{
			short value = *(short*)&m_Data[i++];
			indices.push_back(value);

			i += 1;
		}
	}
	else
		Log::Warning("Unrecognized mesh index type: " + std::to_string(componentType));

	return indices;
}

std::vector<uint8_t> ModelLoader::m_GetUBytes(const nlohmann::json& accessor)
{
	ZoneScoped;

	std::vector<uint8_t> ubytesVec;

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
	uint32_t lengthOfData = count * numPerVert;
	for (uint32_t i = beginningOfData; i < beginningOfData + lengthOfData; i)
		ubytesVec.push_back(*(uint8_t*)&m_Data[i++]);

	return ubytesVec;
}

ModelLoader::MeshMaterial ModelLoader::m_GetMaterial(const nlohmann::json& Primitive)
{
	ZoneScoped;

	TextureManager* texManager = TextureManager::Get();

	ModelLoader::MeshMaterial material;
	material.BaseColorTexture = texManager->LoadTextureFromPath("textures/white.png");

	auto materialIdIt = Primitive.find("material");

	if (materialIdIt == Primitive.end())
		return material;

	int materialId = materialIdIt.value();

	const nlohmann::json& materialDescription = m_JsonData["materials"][materialId];

	static std::unordered_map<std::string_view, MeshMaterial::MaterialAlphaMode> NameToAlphaMode =
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
		// cut off `resources/`
		m_File.substr(10, m_File.size() - 10),
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
	const std::vector<glm::vec3>& Positions,
	const std::vector<glm::vec3>& Normals,
	const std::vector<glm::vec2>& TexUVs,
	const std::vector<glm::vec4>& Colors,
	const std::vector<glm::tvec4<uint8_t>>& Joints,
	const std::vector<glm::vec4>& Weights
)
{
	ZoneScoped;

	std::vector<Vertex> vertices;
	vertices.reserve(Positions.size());

	if (Joints.size() == Positions.size())
		for (int i = 0; i < Positions.size(); i++)
			vertices.emplace_back(
				Positions[i],
				Normals[i],
				Colors[i],
				TexUVs[i],
				std::array<uint8_t, 4>{ Joints[i].x, Joints[i].y, Joints[i].z, Joints[i].w },
				std::array<float, 4>{ Weights[i].x, Weights[i].y, Weights[i].z, Weights[i].w }
			);

	else
		for (int i = 0; i < Positions.size(); i++)
			vertices.emplace_back(
				Positions[i],
				Normals[i],
				Colors[i],
				TexUVs[i],
				std::array<uint8_t, 4>{ UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX },
				std::array<float, 4>{ FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX }
			);

	return vertices;
}

std::vector<glm::vec2> ModelLoader::m_GetAndGroupFloatsVec2(const nlohmann::json& Accessor)
{
	ZoneScoped;

	if (Accessor["type"] != "VEC2")
		throw("Expected accessor to be VEC2");

	std::vector<float> floats = m_GetFloats(Accessor);

	std::vector<glm::vec2> vectors;
	vectors.reserve(static_cast<size_t>(floats.size() / 2));

	for (int i = 0; i < floats.size(); i += 2)
		vectors.emplace_back(
			floats[i+0ull],
			floats[i+1ull]
		);

	return vectors;
}

std::vector<glm::vec3> ModelLoader::m_GetAndGroupFloatsVec3(const nlohmann::json& Accessor)
{
	ZoneScoped;

	if (Accessor["type"] != "VEC3")
		throw("Expected accessor to be VEC3");

	std::vector<float> floats = m_GetFloats(Accessor);

	std::vector<glm::vec3> vectors;
	vectors.reserve(static_cast<size_t>(floats.size() / 3));

	for (int i = 0; i < floats.size(); i += 3)
		vectors.emplace_back(
			floats[i+2ull],
			floats[i+1ull],
			floats[i+0ull]
		);

	return vectors;
}

std::vector<glm::vec4> ModelLoader::m_GetAndGroupFloatsVec4(const nlohmann::json& Accessor)
{
	ZoneScoped;

	std::vector<float> floats = m_GetFloats(Accessor);

	std::vector<glm::vec4> vectors;
	vectors.reserve(static_cast<size_t>(floats.size() / 4));

	if (Accessor["type"] == "VEC4")
		for (int i = 0; i < floats.size(); i += 4)
			vectors.emplace_back(
				floats[i + 3ull],
				floats[i + 2ull],
				floats[i + 1ull],
				floats[i + 0ull]
			);

	else if (Accessor["type"] == "VEC3")
		for (int i = 0; i < floats.size(); i += 3)
			vectors.emplace_back(
				floats[i + 2ull],
				floats[i + 1ull],
				floats[i + 0ull],
				1.f
			);

	else
		throw("Expected accessor to be either VEC3 or VEC4");

	return vectors;
}

std::vector<glm::mat4> ModelLoader::m_GetAndGroupFloatsMat4(const nlohmann::json& Accessor)
{
	ZoneScoped;

	if (Accessor["type"] != "MAT4")
		throw("Expected accessor to be MAT4");

	std::vector<float> floats = m_GetFloats(Accessor);

	std::vector<glm::mat4> mats;
	mats.reserve(static_cast<size_t>(mats.size() / 16));

	for (int i = 0; i < floats.size(); i+=16)
		mats.emplace_back(
			floats[i + 0ull],
			floats[i + 1ull],
			floats[i + 2ull],
			floats[i + 3ull],

			floats[i + 4ull],
			floats[i + 5ull],
			floats[i + 6ull],
			floats[i + 7ull],

			floats[i + 8ull],
			floats[i + 9ull],
			floats[i + 10ull],
			floats[i + 11ull],
			floats[i + 12ull],

			floats[i + 13ull],
			floats[i + 14ull],
			floats[i + 15ull]
		);

	return mats;
}

std::vector<glm::tvec4<uint8_t>> ModelLoader::m_GetAndGroupUBytesVec4(const nlohmann::json& Accessor)
{
	ZoneScoped;

	std::vector<uint8_t> ubytes = m_GetUBytes(Accessor);

	std::vector<glm::tvec4<uint8_t>> vectors;
	vectors.reserve(static_cast<size_t>(ubytes.size() / 4));

	for (int i = 0; i < ubytes.size(); i += 4)
		vectors.emplace_back(
			ubytes[i + 3ull],
			ubytes[i + 2ull],
			ubytes[i + 1ull],
			ubytes[i + 0ull]
		);

	return vectors;
}
