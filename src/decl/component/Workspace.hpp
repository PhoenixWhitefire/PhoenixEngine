#pragma once

#include "datatype/GameObject.hpp"
#include "datatype/Vector3.hpp"

struct EcWorkspace
{
	GameObject* GetSceneCamera() const;
	void SetSceneCamera(GameObject*);
	Vector3 ScreenPointToRay(double x, double y, float length, Vector3* origin) const;

	uint32_t m_SceneCameraId = PHX_GAMEOBJECT_NULL_ID;

	static inline EntityComponent Type = EntityComponent::Workspace;
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
