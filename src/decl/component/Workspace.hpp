#pragma once

#include <glm/vec3.hpp>

#include "datatype/GameObject.hpp"

#define SPATIAL_HASH_GRID_SIZE 32.f

struct SpatialCastResult
{
	GameObject* Object = nullptr;
	glm::vec3 Position;
	glm::vec3 Normal;
	bool Occurred = false;
};

namespace std
{
	template <>
	struct hash<glm::vec3>
	{
		size_t operator()(const glm::vec3& v) const noexcept
		{
			return 0ull
				^ std::hash<float>()(v.x)
				^ std::hash<float>()(v.y)
				^ std::hash<float>()(v.z);
		}
	};

	template <>
	struct hash<glm::ivec3>
	{
		size_t operator()(const glm::ivec3& v) const noexcept
		{
			return 0
				^ std::hash<int>()(v.x)
				^ std::hash<int>()(v.y)
				^ std::hash<int>()(v.z);
		}
	};
};

struct EcWorkspace : public Component<EntityComponent::Workspace>
{
	void Update() const;

	GameObject* GetSceneCamera() const;
	void SetSceneCamera(GameObject*);

	glm::vec3 ScreenPointToVector(glm::vec2 ScreenPosition, float Length) const;

	SpatialCastResult Raycast(const glm::vec3& Origin, const glm::vec3& Vector, const std::vector<GameObject*>& FilterList, bool FilterIsIgnoreList = true) const;
	std::vector<GameObject*> GetObjectsInAabb(const glm::vec3& Position, const glm::vec3& Size, const std::vector<GameObject*>& IgnoreList) const;

	std::unordered_map<glm::ivec3, std::vector<uint32_t>> SpatialHash;

	uint32_t m_SceneCameraId = PHX_GAMEOBJECT_NULL_ID;
	ObjectRef Object;
	bool Valid = true;
};
