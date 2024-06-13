#pragma once

#include<editor/intersectionlib.hpp>
#include<datatype/Event.hpp>
#include<datatype/GameObject.hpp>
#include<gameobject/MeshObject.hpp>
#include<gameobject/Primitive.hpp>
#include<render/Camera.hpp>

class Editor
{
public:

	Editor();

	void Init(std::shared_ptr<GameObject> Workspace);
	void Update(double DeltaTime, glm::mat4 CameraMatrix);
	void RenderUI();

private:
	std::shared_ptr<GameObject> MyCube;
	std::shared_ptr<Object_Primitive> MyCube3D;

	std::shared_ptr<GameObject> Workspace;

	int hierarchyCurItem = 0;
};
