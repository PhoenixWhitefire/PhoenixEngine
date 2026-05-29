// The Hierarchy Root
// 13/08/2024

#include <lua.h>
#include <lualib.h>
#include <tracy/Tracy.hpp>

#include "component/DataModel.hpp"
#include "component/TreeLink.hpp"
#include "script/ScriptEngine.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

uint32_t DataModelComponentManager::CreateComponent(GameObject* Object)
{
    uint32_t id = ComponentManager<EcDataModel>::CreateComponent(Object);
    m_Components[id].Object = Object;

    return id;
}

void DataModelComponentManager::DeleteComponent(uint32_t Id)
{
    EcDataModel& dm = m_Components.at(Id);
    dm.Close();

    for (lua_State* L : dm.Modules)
        lua_resetthread(L);
    dm.Modules.clear();

    dm.UnbindServices();
	ComponentManager<EcDataModel>::DeleteComponent(Id);
}

const Reflection::StaticPropertyMap& DataModelComponentManager::GetProperties()
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
        ),

        REFLECTION_PROPERTY(
            "VM",
            String,
            [](void* p) -> Reflection::GenericValue
            {
                return static_cast<EcDataModel*>(p)->VM;
            },
            [](void* p, const Reflection::GenericValue& gv)
            {
                EcDataModel* dm = static_cast<EcDataModel*>(p);
                if (dm->Modules.size() > 0)
                    RAISE_RT("Cannot change the VM of {} because Scripts have already been bound!", dm->Object->GetFullName());

                dm->VM = gv.AsString();
            }
        )
    };

    return props;
}

const Reflection::StaticMethodMap& DataModelComponentManager::GetMethods()
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
                    RAISE_RT("'{}' is not a valid Service!", service);

                EcDataModel* dm = static_cast<EcDataModel*>(p);
                EntityComponent it = FindComponentTypeByName(service);

                if (it == EntityComponent::None)
                    RAISE_RT("Could not map Service name '{}' to a Component", service);

                if (GameObject* preExisting = dm->Object->FindChildWithComponent(it))
                    return { preExisting->ToGenericValue() };

                ObjectHandle newObject = GameObjectManager::s_Create(it);
                newObject->SetParent(dm->Object);

                return { newObject->ToGenericValue() };
            }
        } },

        { "BindToClose", {
            { Reflection::ValueType::Function },
            {},
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                EcDataModel* dm = static_cast<EcDataModel*>(p);
                if (dm->CloseCallback.Func)
                    RAISE_RT("Cannot overwrite Close callback of datamodel {}", dm->Object->GetFullName());

                dm->CloseCallback = inputs[0].AsFunction();
                return {};
            }
        } },

        { "Close", Reflection::MethodDescriptor{
            { REFLECTION_OPTIONAL(Integer) },
            {},
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                EcDataModel* dm = static_cast<EcDataModel*>(p);

                int64_t exitCode = inputs.size() > 0 ? (int)inputs[0].AsInteger() : 0;

                if (exitCode >= INT32_MIN && exitCode <= INT32_MAX)
                    dm->ExitCode = (int)exitCode;
                else
                    RAISE_RT("Exit code '{}' is out of bounds for a 32-bit integer ([{}, {}])", exitCode, INT32_MIN, INT32_MAX);

                dm->Closed = true;
                REFLECTION_SIGNAL_EVENT(dm->ClosingCallbacks);

                return {};
            }
        } },
    };

    return methods;
}

// TODO: report bug
#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

const Reflection::StaticEventMap& DataModelComponentManager::GetEvents()
{
    static Reflection::StaticEventMap events = {
        REFLECTION_EVENT(EcDataModel, OnFrameBegin, Reflection::ValueType::Double),
        REFLECTION_EVENT(EcDataModel, Closing),
    };

    return events;
}

#ifdef __GNUG__
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#endif

static lua_State* loadModule(const std::string& Module, EcDataModel* Dm)
{
    std::string modulePath = Module;
    bool isAotBytecode = false;

    if (!std::filesystem::is_regular_file(modulePath))
    {
        if (std::filesystem::is_regular_file(modulePath + "c")) // if the path ends in `.luau`, check `.luauc`
            modulePath.append("c");
        else if (std::filesystem::is_regular_file(modulePath + ".luau"))
            modulePath.append(".luau");
        else if (std::filesystem::is_regular_file(modulePath + ".luauc"))
            modulePath.append(".luauc");
    }

    if (modulePath.find(".luauc") == modulePath.size() - strlen(".luauc"))
        isAotBytecode = true;

    bool readSuccess = true;
	std::string sourceCodeOrBytecode = FileRW::ReadFile(modulePath, &readSuccess);

	if (!readSuccess)
	{
		Log.ErrorF(
			"Failed to load '{}': {}",
			Module, sourceCodeOrBytecode // `sourceCodeOrBytecode` will be the error message
		);

		return nullptr;
	}

    ScriptEngine::LuauVM& lvm = ScriptEngine::VMs.at(Dm->VM);
    lua_State* mainThread = lvm.MainThread;

    double allowedExecTime = lvm.AllowedExecutionTime;
    lvm.AllowedExecutionTime = 1.0;

    lua_State* L = lua_newthread(mainThread);
	luaL_sandboxthread(L);

    Logging::ScopedContext sc = Logging::Context{ .ContextExtraTags = std::format("TextDocument:{}", Module) };
    std::string bytecode;

    if (isAotBytecode)
        bytecode = sourceCodeOrBytecode;
    else
        bytecode = ScriptEngine::CompileBytecode(sourceCodeOrBytecode);

    int result = ScriptEngine::LoadBytecode(L, bytecode, "@" + FileRW::ResolvePathNormalized(Module));

	if (result == 0)
	{
        ZoneScopedN("ResumeMain");
        ZoneText(Module.data(), Module.size());

        ScriptEngine::L::PushGenericValue(L, Dm->Object->ToGenericValue());
        lua_setglobal(L, "game");

        if (GameObject* wp = Dm->Object->FindChildWithComponent(EntityComponent::Workspace))
        {
            ScriptEngine::L::PushGenericValue(L, wp->ToGenericValue());
            lua_setglobal(L, "workspace");
        }

		int resumeResult = ScriptEngine::L::Resume(L, nullptr, 0);

		if (resumeResult != LUA_OK && resumeResult != LUA_YIELD)
		{
            lua_Debug ar = {};
            lua_getinfo(L, 1, "l", &ar);

            const char* err = lua_tostring(L, -1);

			Log.Error(
                std::format("DataModel Script init: {}", err ? err : "unknown error"),
                std::format("TextDocument:{},DocumentLine:{}", Module, ar.currentline)
            );
            lua_pop(L, 1);
			ScriptEngine::L::DumpStacktrace(L);

            lua_resetthread(L);
            lua_pop(mainThread, 1);

            lvm.AllowedExecutionTime = allowedExecTime;
            return nullptr;
		}
        else
        {
            lvm.AllowedExecutionTime = allowedExecTime;
            return L;
        }
	}
	else
	{
		Log.ErrorF(
			"DataModel Script compilation: {}",
			lua_tostring(L, -1)
		);

		lua_resetthread(L);
        lua_pop(mainThread, 1);

        lvm.AllowedExecutionTime = allowedExecTime;
        return nullptr;
	}
}

void EcDataModel::Bind()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

    if (Modules.size() > 0 || LiveScripts.empty() || !CanLoadModules)
        return;

    std::string fullName = Object->GetFullName();
    ZoneTextF("DataModel: %s\nScripts: %s", fullName.c_str(), LiveScripts.c_str());

    std::string path = FileRW::ResolvePathNormalized(LiveScripts);

    if (!std::filesystem::is_directory(path))
    {
        lua_State* script = loadModule(path, this);
        if (script)
            Modules.push_back(script);
        else
            CanLoadModules = false;
    }
    else
    {
        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (std::filesystem::is_regular_file(entry))
            {
                lua_State* script = loadModule(entry.path().string(), this);
                if (script)
                    Modules.push_back(script);
                else
                    CanLoadModules = false;
            }
        }
    }
}

void EcDataModel::Close()
{
    if (CloseCallback.Func)
    {
        REFLECTION_SIGNAL_EVENT(ClosingCallbacks);

        (*CloseCallback.Func)({});
        (*CloseCallback.Cleanup)();
        delete CloseCallback.Func;
        delete CloseCallback.Cleanup;

        CloseCallback.Func = nullptr;
        CloseCallback.Cleanup = nullptr;
        CloseCallback.Reference = INT32_MAX;
    }
}

void DataModelComponentManager::NotifyAllOfShutdown()
{
    for (EcDataModel& dm : m_Components)
    {
        if (dm.Valid)
            dm.Close();
    }
}

static void bindServices(const ObjectHandle& Root, std::vector<EntityComponent>& BoundServices)
{
    EcTreeLink* treeLink = nullptr;

    for (const ObjectHandle& obj : Root->GetChildren())
    {
        if (EcTreeLink* tl = obj->FindComponent<EcTreeLink>())
        {
            treeLink = tl;
            break;
        }
    }

    if (treeLink && treeLink->Target.IsValid())
    {
        bindServices(treeLink->Target.Referred(), BoundServices);
    }
    else
    {
        for (const ObjectHandle& obj : Root->GetChildren())
        {
            for (ReflectorRef& ref : obj->Components)
            {
                GetComponentManagerByComponentType(ref.Type)->BindService(ref.Id);

                if (std::find(BoundServices.begin(), BoundServices.end(), ref.Type) == BoundServices.end())
                    BoundServices.push_back(ref.Type);

                if (ref.Type == EntityComponent::TreeLink)
                    treeLink = ref.Get<EcTreeLink>();
            }
        }
    }
}

void EcDataModel::BindServices()
{
    bindServices(Object, BoundServices);
}

void EcDataModel::UnbindServices()
{
    for (EntityComponent ec : BoundServices)
        GetComponentManagerByComponentType(ec)->UnbindService();

    BoundServices.clear();
}
