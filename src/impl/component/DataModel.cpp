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

        for (lua_State* L : dm.Modules)
            lua_resetthread(L);
        dm.Modules.clear();

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
                "LiveScripts",
                String,
                [](void* p) -> Reflection::GenericValue
                {
                    return static_cast<EcDataModel*>(p)->LiveScripts;
                },
                [](void* p, const Reflection::GenericValue& gv)
                {
                    EcDataModel* dm = static_cast<EcDataModel*>(p);
                    if (dm->Modules.size() > 0)
                        RAISE_RT("`LiveScripts` of a DataModel cannot be changed after it is bound!");

                    dm->LiveScripts = gv.AsString();
                    dm->CanLoadModules = true;
                }
            ),

            REFLECTION_PROPERTY(
                "AreScriptsBound",
                Boolean,
                [](void* p) -> Reflection::GenericValue
                {
                    return static_cast<EcDataModel*>(p)->Modules.size() > 0;
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
                    std::string service = std::string(inputs[0].AsStringView());
                    bool validService = false;

                    for (const std::string_view& serv : s_DataModelServices)
                        if (serv == service)
                        {
                            validService = true;
                            break;
                        }

                    if (!validService)
                        RAISE_RTF("'{}' is not a valid Service!", service);

                    EcDataModel* dm = static_cast<EcDataModel*>(p);
                    EntityComponent it = FindComponentTypeByName(service);

                    if (it == EntityComponent::None)
                        RAISE_RTF("Could not map Service name '{}' to a Component", service);

                    if (GameObject* preExisting = dm->Object->FindChildWithComponent(it))
                        return { preExisting->ToGenericValue() };

                    GameObject* newObject = GameObject::Create(it);
                    newObject->SetParent(dm->Object);

                    return { newObject->ToGenericValue() };
                }
            } }
        };

        return methods;
    }

// TODO: report bug
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"

    const Reflection::StaticEventMap& GetEvents() override
    {
        static Reflection::StaticEventMap events = {
            REFLECTION_EVENT(EcDataModel, OnFrameBegin, Reflection::ValueType::Double)
        };

        return events;
    }

#pragma GCC diagnostic pop
};

static inline DataModelManager Instance;

static lua_State* loadModule(const std::string& Module, GameObject* Object)
{
    bool readSuccess = true;
	std::string source = FileRW::ReadFile(Module, &readSuccess);

	if (!readSuccess)
	{
		Log::ErrorF(
			"Failed to load '{}': {}",
			Module, source // `source` will be the error message
		);

		return nullptr;
	}

    lua_State* mainThread = ScriptEngine::GetCurrentVM().MainThread;
    lua_State* L = lua_newthread(mainThread);
	luaL_sandboxthread(L);

	int result = ScriptEngine::CompileAndLoad(L, source, "@" + FileRW::MakePathCwdRelative(Module));

	if (result == 0)
	{
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
            lua_pop(mainThread, 1);

            return nullptr;
		}
        else
            return L;
	}
	else
	{
		Log::ErrorF(
			"DataModel Script compilation: {}",
			lua_tostring(L, -1)
		);

		lua_resetthread(L);
        lua_pop(mainThread, 1);

        return nullptr;
	}
}

void EcDataModel::Bind()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

    if (Modules.size() > 0 || LiveScripts.empty() || !CanLoadModules)
    {
        return;
    }

	std::string fullName = Object->GetFullName();
	ZoneTextF("DataModel: %s\nScripts: %s", fullName.c_str(), LiveScripts.c_str());

	// prevent ourselves from being deleted by the code we run.
	// if that code errors, it'll be a use-after-free as we try
	// to access `m_L` to get the error message
	// 24/12/2024
	ObjectHandle dontKillMePlease = this->Object;

    std::string path = FileRW::MakePathCwdRelative(LiveScripts);

    if (std::filesystem::is_regular_file(path))
    {
        lua_State* script = loadModule(path, Object);
        if (script)
            Modules.push_back(script);
        else
            CanLoadModules = false;
    }
    else if (std::filesystem::is_directory(path))
    {
        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (std::filesystem::is_regular_file(entry))
            {
                lua_State* script = loadModule(entry.path().string(), Object);
                if (script)
                    Modules.push_back(script);
                else
                    CanLoadModules = false;
            }
        }
    }
    else
    {
        Log::ErrorF("Invalid `LiveScripts` path '{}' ({}), expected to be a file or directory", LiveScripts, path);
        CanLoadModules = false;
    }
}
