#pragma once

#include<glm/matrix.hpp>

#include"datatype/GameObject.hpp"

class Object_Model : public GameObject
{
public:
	Object_Model();

	glm::mat4 Matrix = glm::mat4(1.0f);

private:
	static DerivedObjectRegister<Object_Model> RegisterClassAs;
};
