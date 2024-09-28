#pragma once

#include"datatype/GameObject.hpp"

class Object_Model : public GameObject
{
public:
	Object_Model();

	PHX_GAMEOBJECT_API_REFLECTION;

private:
	static RegisterDerivedObject<Object_Model> RegisterClassAs;
	static void s_DeclareReflections();
	static inline Api s_Api{};
};
