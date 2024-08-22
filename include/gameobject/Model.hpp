#pragma once

#include"datatype/GameObject.hpp"

class Object_Model : public GameObject
{
public:
	Object_Model();

private:
	static RegisterDerivedObject<Object_Model> RegisterClassAs;
	static void s_DeclareReflections();
};
