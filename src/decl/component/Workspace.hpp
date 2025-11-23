#pragma once

#include <glm/vec3.hpp>

#include "datatype/GameObject.hpp"

struct SpatialCastResult
{
	GameObject* Object = nullptr;
	glm::vec3 Position;
	glm::vec3 Normal;
	bool Occurred = false;
};

struct EcWorkspace : public Component<EntityComponent::Workspace>
{
	void Update() const;

	GameObject* GetSceneCamera() const;
	void SetSceneCamera(GameObject*);

	glm::vec3 ScreenPointToRay(double x, double y, float length, glm::vec3* origin) const;

	SpatialCastResult Raycast(const glm::vec3& Origin, const glm::vec3& Vector, const std::vector<GameObject*>& FilterList, bool FilterIsIgnoreList = true) const;
	std::vector<GameObject*> GetObjectsInAabb(const glm::vec3& Position, const glm::vec3& Size, const std::vector<GameObject*>& IgnoreList) const;

	uint32_t m_SceneCameraId = PHX_GAMEOBJECT_NULL_ID;
	ObjectRef Object;
	bool Valid = true;
};

/*
class Object_Workspace : public Object_Model
{
public:
	Object_Workspace();

	void Initialize() override;

	Object_Camera* GetSceneCamera() const;
	void SetSceneCamera(Object_Camera*);

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;

	uint32_t m_SceneCameraId = UINT32_MAX;
};
*/
