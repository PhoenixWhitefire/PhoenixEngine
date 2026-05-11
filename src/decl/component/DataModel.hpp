// The Hierarchy Root
// 13/08/2024

#pragma once

#include "component/Workspace.hpp"
#include "script/ScriptEngine.hpp"

struct lua_State;

struct EcDataModel : public Component<EntityComponent::DataModel>
{
	void Bind();
	void Close();
	void BindServices();
	void UnbindServices();

	std::string LiveScripts = "scripts/Main.luau";
	std::string VM = ROOT_LVM_NAME;
	uint32_t Workspace = UINT32_MAX;
	ObjectRef Object;

	std::vector<Reflection::EventCallback> OnFrameBeginCallbacks;
	std::vector<Reflection::EventCallback> ClosingCallbacks;
	Reflection::GenericFunction CloseCallback;
	std::vector<lua_State*> Modules;
	std::vector<EntityComponent> BoundServices;

	int ExitCode = 0;
	bool Closed = false;
	bool CanLoadModules = true;
	bool Valid = true;
};

class DataModelComponentManager : public ComponentManager<EcDataModel>
{
public:
	uint32_t CreateComponent(GameObject* Object) override;
	void DeleteComponent(uint32_t Id) override;
	const Reflection::StaticPropertyMap& GetProperties() override;
	const Reflection::StaticMethodMap& GetMethods() override;
	const Reflection::StaticEventMap& GetEvents() override;

	void NotifyAllOfShutdown();
};
