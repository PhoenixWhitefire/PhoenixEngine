#pragma once

#include<editor/intersectionlib.hpp>
#include<datatype/Event.hpp>
#include<datatype/GameObject.hpp>
#include<gameobject/Mesh3D.hpp>

class Editor
{
public:

	Editor();

	void Init(std::shared_ptr<GameObject> Workspace);
	void Tick(double DeltaTime, glm::mat4 CameraMatrix);

private:
	std::shared_ptr<GameObject> MyCube;
	std::shared_ptr<Object_Mesh3D> MyCube3D;

	std::shared_ptr<GameObject> Workspace;
};
