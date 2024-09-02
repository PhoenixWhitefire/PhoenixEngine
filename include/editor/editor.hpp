#pragma once

#include<datatype/GameObject.hpp>
#include<gameobject/Primitive.hpp>

class Editor
{
public:

	Editor();

	void Init();
	void Update(double DeltaTime, glm::mat4 CameraTransform);
	void RenderUI();

private:

	int hierarchyCurItem = 0;
};
