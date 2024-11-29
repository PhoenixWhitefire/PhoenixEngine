#pragma once

#include "datatype/GameObject.hpp"

class Object_Model : public GameObject
{
public:
	Object_Model();

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};
