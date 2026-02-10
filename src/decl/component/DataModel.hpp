// The Hierarchy Root
// 13/08/2024

#pragma once

#include "component/Workspace.hpp"
#include "script/ScriptEngine.hpp"

struct lua_State;

struct EcDataModel : public Component<EntityComponent::DataModel>
{
	void Bind();

	std::string LiveScripts = "scripts/live";
	std::string VM = ROOT_LVM_NAME;
	uint32_t Workspace = UINT32_MAX;
	ObjectRef Object;

	std::vector<Reflection::EventCallback> OnFrameBeginCallbacks;
	std::vector<lua_State*> Modules;
	bool CanLoadModules = true;
	bool Valid = true;
};
