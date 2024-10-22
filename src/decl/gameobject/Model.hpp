#pragma once

#include "datatype/GameObject.hpp"

class Object_Model : public GameObject
{
public:
	Object_Model();

	PHX_GAMEOBJECT_API_REFLECTION;

private:
	static void s_DeclareReflections();
	static inline Api s_Api{};
};
