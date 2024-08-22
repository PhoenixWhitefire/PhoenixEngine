#pragma once

#include"gameobject/Model.hpp"
#include"gameobject/Camera.hpp"
#include"datatype/Vector2.hpp"

class Object_Workspace : public Object_Model
{
public:
	Object_Workspace();

	void Initialize();

	Object_Camera* GetSceneCamera();
	void SetSceneCamera(Object_Camera*);

private:

	static RegisterDerivedObject<Object_Workspace> RegisterClassAs;
	static void s_DeclareReflections();
	static bool s_DidInitReflection;

	Object_Camera* m_sceneCamera;
};
