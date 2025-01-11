// The Hierarchy Root
// 13/08/2024

#pragma once

#include "datatype/GameObject.hpp"
#include "gameobject/Workspace.hpp"

class Object_DataModel : public GameObject
{
public:
	Object_DataModel();
	~Object_DataModel() override;
	
	void Initialize() override;

	Object_Workspace* GetWorkspace();

	bool WantExit = false;

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};
