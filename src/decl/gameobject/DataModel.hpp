// The Hierarchy Root
// 13/08/2024

#pragma once

#include "datatype/GameObject.hpp"

class Object_DataModel : public GameObject
{
public:
	Object_DataModel();
	~Object_DataModel() override;
	
	void Initialize() override;

	bool WantExit = false;

	PHX_GAMEOBJECT_API_REFLECTION;

private:
	static void s_DeclareReflections();
	static inline Api s_Api{};
};
