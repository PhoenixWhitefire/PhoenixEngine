#pragma once

#include"gameobject/Model.hpp"
#include"gameobject/Camera.hpp"
#include"datatype/Vector2.hpp"

class Object_Workspace : public GameObject
{
public:
	Object_Workspace();

	std::shared_ptr<Object_Camera> GetSceneCamera();
	void SetSceneCamera(std::shared_ptr<Object_Camera>);

private:

	std::shared_ptr<Object_Camera> m_sceneCamera;

	static DerivedObjectRegister<Object_Workspace> RegisterClassAs;
};
