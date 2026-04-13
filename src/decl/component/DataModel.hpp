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

	std::string LiveScripts = "scripts/live";
	std::string VM = ROOT_LVM_NAME;
	uint32_t Workspace = UINT32_MAX;
	ObjectRef Object;

	std::vector<Reflection::EventCallback> OnFrameBeginCallbacks;
	Reflection::GenericFunction CloseCallback;
	std::vector<lua_State*> Modules;
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
