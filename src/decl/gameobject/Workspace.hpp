#pragma once

#include "gameobject/Model.hpp"
#include "gameobject/Camera.hpp"
#include "datatype/Vector2.hpp"

class Object_Workspace : public Object_Model
{
public:
	Object_Workspace();

	void Initialize() override;

	Object_Camera* GetSceneCamera();
	void SetSceneCamera(Object_Camera*);

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;

	Object_Camera* m_SceneCamera;
};
