// The Hierarchy Root
// 13/08/2024

#include <lua.h>
#include <lualib.h>
#include <tracy/Tracy.hpp>

#include "component/DataModel.hpp"
#include "script/ScriptEngine.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

class DataModelManager : public ComponentManager<EcDataModel>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        uint32_t id = ComponentManager<EcDataModel>::CreateComponent(Object);
        m_Components.back().Object = Object;

        return id;
    }

    void DeleteComponent(uint32_t Id) override
    {
        EcDataModel& dm = m_Components.at(Id);
        if (dm.ModuleData)
        {
            lua_resetthread(dm.ModuleData);
            dm.ModuleData = nullptr;
        }

		ComponentManager<EcDataModel>::DeleteComponent(Id);
    }

    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
            REFLECTION_PROPERTY(
                "Time",
                Double,
                [](void*)
                -> Reflection::GenericValue
                {
                    return GetRunningTime();
                },
                nullptr
            ),

            REFLECTION_PROPERTY(
                "Module",
                String,
                [](void* p) -> Reflection::GenericValue
                {
                    return static_cast<EcDataModel*>(p)->Module;
                },
                [](void* p, const Reflection::GenericValue& gv)
                {
                    EcDataModel* dm = static_cast<EcDataModel*>(p);
                    if (dm->ModuleData)
                        RAISE_RT("Module of a DataModel cannot be changed after it is bound!");

                    dm->Module = gv.AsString();
                    dm->CanLoadModule = true;
                }
            ),

            REFLECTION_PROPERTY(
                "IsModuleBound",
                Boolean,
                [](void* p) -> Reflection::GenericValue
                {
                    return static_cast<EcDataModel*>(p)->ModuleData != nullptr;
                },
                nullptr
            )
        };

        return props;
    }

    const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap methods = {
            { "GetService", {
                { Reflection::ValueType::String },
                { Reflection::ValueType::GameObject },
                [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    const std::string_view Services[] = {
                        "Workspace",
                        "Engine",
                        "InputService",
                        "AssetService",
                        "HistoryService"
                    };

                    std::string service = std::string(inputs[0].AsStringView());
                    bool validService = false;

                    for (const std::string_view& serv : Services)
                        if (serv == service)
                        {
                            validService = true;
                            break;
                        }

                    if (!validService)
                        RAISE_RTF("'{}' is not a valid Service!", service);

                    EcDataModel* dm = static_cast<EcDataModel*>(p);
                    const auto& it = s_ComponentNameToType.find(service);

                    if (it == s_ComponentNameToType.end())
                        RAISE_RTF("Could not map Service name '{}' to a Component", service);

                    if (GameObject* preExisting = dm->Object->FindChildWithComponent(it->second))
                        return { preExisting->ToGenericValue() };

                    GameObject* newObject = GameObject::Create(it->second);
                    newObject->SetParent(dm->Object);

                    return { newObject->ToGenericValue() };
                }
            } }
        };

        return methods;
    }

    const Reflection::StaticEventMap& GetEvents() override
    {
        static Reflection::StaticEventMap events = {
            REFLECTION_EVENT(EcDataModel, OnFrameBegin, Reflection::ValueType::Double)
        };

        return events;
    }
};

static inline DataModelManager Instance;

void EcDataModel::Bind()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

    if (ModuleData || Module.empty() || !CanLoadModule)
    {
        return;
    }

	std::string fullName = Object->GetFullName();
	ZoneTextF("DataModel: %s\nModule: %s", fullName.c_str(), Module.c_str());

	bool readSuccess = true;
	std::string source = FileRW::ReadFile(Module, &readSuccess);

	if (!readSuccess)
	{
		Log::ErrorF(
			"Failed to load '{}' for DataModel {}: {}",
			Module, fullName, source // `source` will be the error message
		);
        CanLoadModule = false;

		return;
	}

    lua_State* L = lua_newthread(ScriptEngine::GetCurrentVM().MainThread);
	luaL_sandboxthread(L);

	int result = ScriptEngine::CompileAndLoad(L, source, "@" + FileRW::MakePathCwdRelative(Module));

	if (result == 0)
	{
		// prevent ourselves from being deleted by the code we run.
		// if that code errors, it'll be a use-after-free as we try
		// to access `m_L` to get the error message
		// 24/12/2024
		ObjectHandle dontKillMePlease = this->Object;

        ScriptEngine::L::PushGenericValue(L, Object->ToGenericValue());
        lua_setglobal(L, "game");

        if (GameObject* wp = Object->FindChildWithComponent(EntityComponent::Workspace))
        {
            ScriptEngine::L::PushGenericValue(L, wp->ToGenericValue());
            lua_setglobal(L, "workspace");
        }

		int resumeResult = ScriptEngine::L::Resume(L, nullptr, 0);

		if (resumeResult != LUA_OK && resumeResult != LUA_YIELD)
		{
			Log::ErrorF(
				"DataModel Module init: {}",
				lua_tostring(L, -1)
			);
			ScriptEngine::L::DumpStacktrace(L);

            lua_resetthread(L);
		}
        else
            ModuleData = L;
	}
	else
	{
		Log::ErrorF(
			"DataModel Module compilation: {}",
			lua_tostring(L, -1)
		);

		lua_resetthread(L);
	}
}
