// The Hierarchy Root
// 13/08/2024

#pragma once

#include "component/Workspace.hpp"

struct EcDataModel
{
	bool WantExit = false;
	uint32_t Workspace = UINT32_MAX;

	static inline EntityComponent Type = EntityComponent::DataModel;
};

/*
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
*/
