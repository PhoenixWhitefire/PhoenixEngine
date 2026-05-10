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

                ScriptEngine::L::Close(it->second.MainThread);
                ScriptEngine::VMs.erase(it);

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

	            lua_State* ML = lua_newthread(vm.MainThread);
                luaL_sandboxthread(ML);

                if (ScriptEngine::CompileAndLoad(ML, code, chname) == 0)
                {
                    int result = lua_resume(ML, ML, 0);
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
            { Reflection::ValueType::String },
            { Reflection::ValueType::Buffer },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                std::string bytecode = ScriptEngine::CompileBytecode(inputs[0].AsStringView());
                Reflection::GenericValue retval = bytecode;
                retval.Type = Reflection::ValueType::Buffer;

                return { retval };
            }
        } },
    };

    return methods;
}
