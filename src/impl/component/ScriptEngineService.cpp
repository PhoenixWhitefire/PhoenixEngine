// ScriptEngine service, 10/05/2026
#include <lualib.h>

#include "component/ScriptEngineService.hpp"
#include "script/ScriptEngine.hpp"

const Reflection::StaticMethodMap& ScriptEngineComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap methods = {
        { "CreateVM", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                ScriptEngine::RegisterNewVM(std::string(inputs[0].AsStringView()));

                return {};
            }
        } },

        { "CloseVM", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const auto& it = ScriptEngine::VMs.find(std::string(inputs[0].AsStringView()));
                if (it == ScriptEngine::VMs.end())
                    RAISE_RT("Invalid VM");

                it->second.Close();
                return {};
            }
        } },

        { "RunInVM", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, Reflection::ValueType::String, REFLECTION_OPTIONAL(String) },
            { Reflection::ValueType::Boolean, REFLECTION_OPTIONAL(String) },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const std::string vmName = std::string(inputs[0].AsStringView());

                const auto& it = ScriptEngine::VMs.find(vmName);
                if (it == ScriptEngine::VMs.end())
                    RAISE_RT("Invalid VM '{}'", vmName);

                const ScriptEngine::LuauVM& vm = it->second;

                const std::string code = std::string(inputs[1].AsStringView());
	            const std::string chname = inputs.size() > 2 ? std::string(inputs[2].AsStringView()) : code;
                Logging::ScopedContext sc = Logging::Context{ .ContextExtraTags = std::format("SourceChunkName:{}", chname) };

                ScriptEngine::L::StateUserdata* vmud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(vm.MainThread);
                vmud->LastResumed = GetRunningTime(); // should do this before `luaL_sandboxthread` because that can trigger GC

	            lua_State* ML = lua_newthread(vm.MainThread);
                luaL_sandboxthread(ML);

                if (ScriptEngine::CompileAndLoad(ML, code, chname) == 0)
                {
                    int result = ScriptEngine::L::Resume(ML, ML, 0);
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
            { Reflection::ValueType::String, REFLECTION_OPTIONAL(Integer), REFLECTION_OPTIONAL(Integer) },
            { Reflection::ValueType::Buffer },
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
            { Reflection::ValueType::String, Reflection::ValueType::Double },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const std::string_view& vmName = inputs[0].AsStringView();

                const auto& vmit = ScriptEngine::VMs.find(std::string(vmName));
                ScriptEngine::LuauVM* lvm = nullptr;

                if (vmit == ScriptEngine::VMs.end())
                {
                    const auto& pvmit = std::find_if(ScriptEngine::ParallelVMs.begin(), ScriptEngine::ParallelVMs.end(), [vmName](const ScriptEngine::ParallelVM* pvm)
                    {
                        return pvm->Name == vmName;
                    });

                    if (pvmit != ScriptEngine::ParallelVMs.end())
                        lvm = *pvmit;
                    else
                        RAISE_RT("Invalid VM '{}'", vmName);
                }
                else
                    lvm = &vmit->second;

                lvm->AllowedExecutionTime = inputs[1].AsDouble();
                return {};
            }
        } },


        { "DetachDebuggerFromVM", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const std::string& vmName = inputs[0].AsString();

                const auto& vmit = ScriptEngine::VMs.find(vmName);
                ScriptEngine::LuauVM* lvm = nullptr;

                if (vmit == ScriptEngine::VMs.end())
                {
                    const auto& pvmit = std::find_if(ScriptEngine::ParallelVMs.begin(), ScriptEngine::ParallelVMs.end(), [vmName](const ScriptEngine::ParallelVM* pvm)
                    {
                        return pvm->Name == vmName;
                    });

                    if (pvmit != ScriptEngine::ParallelVMs.end())
                        lvm = *pvmit;
                    else
                        RAISE_RT("Invalid VM '{}'", vmName);
                }
                else
                    lvm = &vmit->second;

                ScriptEngine::L::StateUserdata* vmud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lvm->MainThread);
                vmud->DebuggerAttached = false;

                lua_Callbacks* cb = lua_callbacks(lvm->MainThread);
                cb->debugprotectederror = nullptr;
                cb->debuginterrupt = nullptr;
                cb->debugbreak = nullptr;
                cb->debugstep = nullptr;
                return {};
            }
        } },
    };

    return methods;
}
