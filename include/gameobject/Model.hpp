#pragma once

#include<glm/matrix.hpp>

#include"datatype/GameObject.hpp"

class Object_Model : public GameObject
{
public:
	Object_Model();

private:
	static DerivedObjectRegister<Object_Model> RegisterClassAs;
};
