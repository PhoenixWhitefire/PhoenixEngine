#pragma once

#include "datatype/GameObject.hpp"

class Object_Animation : public GameObject
{
public:
	Object_Animation();

	REFLECTION_DECLAREAPI;

private:
	void s_DeclareReflections();
	static inline Reflection::Api s_Api{};
};
