#pragma once

#include <unordered_map>
#include <vector>

#include "datatype/GameObject.hpp"
#include "datatype/Mesh.hpp"

class ModelLoader
{
public:
	ModelLoader(const std::string& AssetPath, uint32_t Parent);

	std::vector<GameObjectRef> LoadedObjs;

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
		struct BoneInfo
		{
			int32_t NodeId = UINT32_MAX;
			glm::mat4 InvBindMatrix{ 1.f };
		};

		enum class NodeType : uint8_t
		{
			Primitive,
			Container,
			Bone
		};

		std::string Name;
		uint32_t NodeId{ UINT32_MAX };
		uint32_t Parent;

		NodeType Type = NodeType::Primitive;

		Mesh Data;
		MeshMaterial Material;
		glm::mat4 Transform;
		glm::vec3 Scale{ 1.f, 1.f, 1.f };
		std::vector<BoneInfo> Bones;
	};

	ModelLoader() = delete;

	ModelNode m_LoadPrimitive(
		const nlohmann::json& MeshData,
		uint32_t PrimitiveIndex,
		const glm::mat4& Transform,
		const glm::vec3& Scale
	);

	void m_TraverseNode(uint32_t NextNode, uint32_t From, const glm::mat4& Transform = glm::mat4(1.f));

	void m_BuildRig();

	std::string m_GetData();

	std::vector<float> m_GetFloats(const nlohmann::json& Accessor);
	std::vector<uint32_t> m_GetUnsigned32s(const nlohmann::json& Accessor);
	std::vector<uint8_t> m_GetUBytes(const nlohmann::json& Accessor);
	MeshMaterial m_GetMaterial(const nlohmann::json&);

	std::vector<Vertex> m_AssembleVertices(
		const std::vector<glm::vec3>& Positions,
		const std::vector<glm::vec3>& Normals,
		const std::vector<glm::vec2>& TextureUVs,
		const std::vector<glm::vec4>& Colors,
		const std::vector<glm::tvec4<uint8_t>>& Joints,
		const std::vector<glm::vec4>& Weights
	);

	std::vector<glm::vec2> m_GetAndGroupFloatsVec2(const nlohmann::json& Accessor);
	std::vector<glm::vec3> m_GetAndGroupFloatsVec3(const nlohmann::json& Accessor);
	std::vector<glm::vec4> m_GetAndGroupFloatsVec4(const nlohmann::json& Accessor);
	std::vector<glm::mat4> m_GetAndGroupFloatsMat4(const nlohmann::json& Accessor);
	std::vector<glm::tvec4<uint8_t>> m_GetAndGroupUBytesVec4(const nlohmann::json& Accessor);

	std::string m_File;
	nlohmann::json m_JsonData;

	std::vector<ModelNode> m_Nodes;
	std::unordered_map<int32_t, uint32_t> m_NodeIdToIndex;
	std::vector<GameObjectRef> m_Animations;

	std::string m_Data;
};
