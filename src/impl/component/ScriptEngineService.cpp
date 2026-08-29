// ScriptEngine service, 10/05/2026
#include <lualib.h>

#include "component/ScriptEngineService.hpp"
#include "script/ScriptEngine.hpp"

const Reflection::StaticMethodMap& ScriptEngineComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap methods = {
        { "CreateVM", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                ScriptEngine* scriptEngine = ScriptEngine::Get();
                scriptEngine->RegisterNewVM(std::string(inputs[0].AsStringView()));

                return {};
            }
        } },

        { "CloseVM", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                ScriptEngine* scriptEngine = ScriptEngine::Get();
                const auto& it = scriptEngine->VMs.find(std::string(inputs[0].AsStringView()));
                if (it == scriptEngine->VMs.end())
                    RAISE_RT("Invalid VM");

                it->second->Close();
                return {};
            }
        } },

        { "RunInVM", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String, Reflection::ValueType::String, REFLECTION_OPTIONAL(String) }),
            REFLECTION_SPAN({ Reflection::ValueType::Boolean, REFLECTION_OPTIONAL(String) }),
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                ScriptEngine* scriptEngine = ScriptEngine::Get();
                const std::string vmName = std::string(inputs[0].AsStringView());

                const auto& it = scriptEngine->VMs.find(vmName);
                if (it == scriptEngine->VMs.end())
                    RAISE_RT("Invalid VM '{}'", vmName);

                const ScriptEngine::LuauVM& vm = *it->second;

                const std::string code = std::string(inputs[1].AsStringView());
                const std::string chname = inputs.size() > 2 ? std::string(inputs[2].AsStringView()) : code;
                Logging::ScopedContext sc = Logging::Context{ .ContextExtraTags = std::format("SourceChunkName:{}", chname) };

                ScriptEngine::StateUserdata* vmud = (ScriptEngine::StateUserdata*)lua_getthreaddata(vm.MainThread);
                vmud->LastResumed = GetRunningTime(); // should do this before `luaL_sandboxthread` because that can trigger GC

                lua_State* ML = lua_newthread(vm.MainThread);
                luaL_sandboxthread(ML);

                if (ScriptEngine::CompileAndLoad(ML, code, chname) == 0)
                {
                    int result = ScriptEngine::Resume(ML, ML, 0);
                    const char* err = nullptr;

                    if (result == LUA_ERRERR || result == LUA_ERRRUN || result == LUA_ERRMEM)
                    {
                        err = lua_tostring(ML, -1); // metatable check in `luaL_tolstring` can trigger an assertion `lua_getmetatable` `api_incr_top`
                        if (!err)
                            err = "unknown error";
                        Log.Error(err);
                    }

                    const char* const ResultToMessage[] = {
                        "ok",
                        "yield",
                        nullptr,
                        nullptr,
                        nullptr,
                        nullptr,
                        "break"
                    };

                    std::string message;
                    if (const char* mes = ResultToMessage[result])
                        message = mes;
                    else
                        message = err;

                    lua_pop(vm.MainThread, 1); // pop off ML

                    return { result == LUA_OK, message };
                }
                else
                {
                    std::string message = lua_tostring(ML, -1);
                    lua_pop(vm.MainThread, 1); // pop off ML

                    Log.Error(message);
                    return { false, message };
                }
            }
        } },

        { "CompileToBytecode", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String, REFLECTION_OPTIONAL(Integer), REFLECTION_OPTIONAL(Integer) }),
            REFLECTION_SPAN({ Reflection::ValueType::Buffer }),
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                int optLevel = inputs.size() > 1 ? (int)inputs[1].AsInteger() : -1;
                int debugLevel = inputs.size() > 2 ? (int)inputs[2].AsInteger() : -1;

                std::string bytecode = ScriptEngine::CompileBytecode(inputs[0].AsStringView(), optLevel, debugLevel);
                Reflection::GenericValue retval = bytecode;
                retval.Type = Reflection::ValueType::Buffer;

                return { retval };
            }
        } },

        { "SetVMAllowedExecutionTime", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String, Reflection::ValueType::Double }),
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                ScriptEngine* scriptEngine = ScriptEngine::Get();
                const std::string_view& vmName = inputs[0].AsStringView();

                const auto& vmit = scriptEngine->VMs.find(std::string(vmName));
                ScriptEngine::LuauVM* lvm = nullptr;

                if (vmit == scriptEngine->VMs.end())
                {
                    const auto& pvmit = std::find_if(scriptEngine->ParallelVMs.begin(), scriptEngine->ParallelVMs.end(), [vmName](const ScriptEngine::ParallelVM* pvm)
                    {
                        return pvm->Name == vmName;
                    });

                    if (pvmit != scriptEngine->ParallelVMs.end())
                        lvm = *pvmit;
                    else
                        RAISE_RT("Invalid VM '{}'", vmName);
                }
                else
                    lvm = vmit->second;

                ScriptEngine::StateUserdata* vmud = (ScriptEngine::StateUserdata*)lua_getthreaddata(lvm->MainThread);
                vmud->AllowedExecutionTime = inputs[1].AsDouble();
                return {};
            }
        } },

        { "DetachDebuggerFromVM", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                ScriptEngine* scriptEngine = ScriptEngine::Get();
                const std::string& vmName = inputs[0].AsString();

                const auto& vmit = scriptEngine->VMs.find(vmName);
                ScriptEngine::LuauVM* lvm = nullptr;

                if (vmit == scriptEngine->VMs.end())
                {
                    const auto& pvmit = std::find_if(scriptEngine->ParallelVMs.begin(), scriptEngine->ParallelVMs.end(), [vmName](const ScriptEngine::ParallelVM* pvm)
                    {
                        return pvm->Name == vmName;
                    });

                    if (pvmit != scriptEngine->ParallelVMs.end())
                        lvm = *pvmit;
                    else
                        RAISE_RT("Invalid VM '{}'", vmName);
                }
                else
                    lvm = vmit->second;

                ScriptEngine::StateUserdata* vmud = (ScriptEngine::StateUserdata*)lua_getthreaddata(lvm->MainThread);
                vmud->DebuggerAttached = false;

                lua_Callbacks* cb = lua_callbacks(lvm->MainThread);
                cb->debugprotectederror = nullptr;
                cb->debuginterrupt = nullptr;
                cb->debugbreak = nullptr;
                cb->debugstep = nullptr;
                return {};
            }
        } },

        { "SetScriptBreakpoint", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String, Reflection::ValueType::String, Reflection::ValueType::Integer, Reflection::ValueType::Boolean }),
            REFLECTION_SPAN({ Reflection::ValueType::Integer }),
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                ScriptEngine* scriptEngine = ScriptEngine::Get();
                int lineApplied = scriptEngine->SetScriptBreakpoint(
                    inputs[0].AsString(),
                    inputs[1].AsString(),
                    (int)inputs[2].AsInteger(),
                    inputs[3].AsBoolean()
                );

                return { lineApplied };
            }
        } },
    };

    return methods;
}

const Reflection::StaticEventMap& ScriptEngineComponentManager::GetEvents()
{
    static const Reflection::StaticEventMap events = {
        REFLECTION_EVENT(EcScriptEngineService, BreakpointMoved, Reflection::ValueType::String, Reflection::ValueType::Integer, Reflection::ValueType::Integer),
    };

    return events;
}

void EcScriptEngineService::SignalBreakpointMoved(const std::vector<Reflection::GenericValue>& Arguments)
{
    ZoneScoped;
    const ScriptEngineComponentManager* manager = (ScriptEngineComponentManager*)ScriptEngineComponentManager::Get();

    for (const EcScriptEngineService& ed : manager->Components)
    {
        if (ed.Valid)
            Reflection::SignalEvent(ed.BreakpointMovedCallbacks, Arguments, "ScriptEngine.BreakpointMoved");
    }
}
