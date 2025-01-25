#pragma once

#include "gameobject/Model.hpp"
#include "gameobject/Camera.hpp"
#include "datatype/Vector2.hpp"

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
