// The Hierarchy Root
// 13/08/2024

#pragma once

#include "component/Workspace.hpp"
#include "Reflection.hpp"

struct lua_State;

struct EcDataModel : public Component<EntityComponent::DataModel>
{
	void Bind();

	std::string Module = "scripts/Main.luau";
	uint32_t Workspace = UINT32_MAX;
	ObjectRef Object;

	std::vector<Reflection::EventCallback> OnFrameBeginCallbacks;
	lua_State* ModuleData = nullptr;
	bool CanLoadModule = true;
	bool Valid = true;
};
