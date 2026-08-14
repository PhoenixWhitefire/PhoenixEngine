#include "Reflection.hpp"
#include <luau/Require/include/Luau/Require.h>
#include <lualib.h>

#ifdef __clang__

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow-field-in-constructor"
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wweak-vtables"
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wunused-template"

#endif

#include <Luau/Compiler.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

#include <Luau/CodeGen.h>

#include "script/ScriptEngine.hpp"
#include "script/UserdataTags.hpp"
#include "script/LightUserdataTags.hpp"
#include "script/SharedMutex.hpp"
#include "script/TracyLuau.hpp"
#include "script/luhx.hpp"
#include "datatype/Color.hpp"
#include "component/ScriptEngineService.hpp"
#include "DeveloperTools.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

struct LuauType
{
    LuauType(lua_Type BaseType)
        : Type(BaseType)
    {}

    LuauType(lua_Type BaseType, UserdataTag UserTag)
        : Type(BaseType), Tag(UserTag)
    {}

    int Type = LUA_TNONE;
    UserdataTag Tag = UserdataTag::invalid;
};

// depends on the ordering of `Reflection::ValueType`!!
static const LuauType s_ValueTypeToLuauType[] = {
    LUA_TNIL,

    LUA_TBOOLEAN,
    LUA_TNUMBER, // Integer
    LUA_TNUMBER, // Double
    LUA_TSTRING,
    LUA_TBUFFER,

    { LUA_TUSERDATA, UserdataTag::Color      },
    { LUA_TVECTOR,   UserdataTag::invalid  }, // Vector2
    { LUA_TVECTOR,   UserdataTag::invalid  }, // Vector3
    { LUA_TUSERDATA, UserdataTag::Matrix     },
    { LUA_TUSERDATA, UserdataTag::GameObject },

    LUA_TFUNCTION,

    LUA_TTABLE, // Array
    LUA_TTABLE, // Map,

    { LUA_TUSERDATA, UserdataTag::EventSignal    },
    { LUA_TUSERDATA, UserdataTag::InputEvent     },
    { LUA_TUSERDATA, UserdataTag::NumberGradient },
    { LUA_TUSERDATA, UserdataTag::VectorGradient },
    { LUA_TUSERDATA, UserdataTag::ColorGradient  },
};

static_assert(std::size(s_ValueTypeToLuauType) == Reflection::ValueType::lastBase);

static int luauAssertHandler(const char* expression, const char* file, int line, const char* function)
{
    Log.ErrorF("Luau assertion failed:\n\tExpression: {}\n\tIn: {}:{} in {}", expression, file, line, function);
    assert(false);
    RAISE_RT("Luau assertion failed:\n\tExpression: {}\n\tIn: {}:{} in {}", expression, file, line, function);
}

void ScriptEngine::Initialize()
{
    RegisterNewVM(ROOT_LVM_NAME);

    // changing a reference to a static function variable
    Luau::assertHandler() = luauAssertHandler;
}

void ScriptEngine::Shutdown()
{
    ZoneScoped;

    std::vector<LuauVM*> vms;
    vms.reserve(VMs.size());

    for (auto& [_, vm] : VMs)
        vms.push_back(&vm);

    for (LuauVM* vm : vms)
        vm->Close();

    assert(VMs.size() == 0);

    while (ParallelVMsExecuting != 0)
        std::this_thread::sleep_for(std::chrono::microseconds(100));

    for (ParallelVM* vm : ParallelVMs)
    {
        vm->Close();
        delete vm;
    }

    ParallelVMs.clear();
    CollectParallelResourceGarbage();
}

ScriptEngine::LuauVM& ScriptEngine::RegisterNewVM(const std::string& Name)
{
    const auto& it = VMs.find(Name);
    if (it != VMs.end())
        RAISE_RT("A VM already exists with that name");

    VMs[Name] = LuauVM{
        .Name = Name,
        .MainThread = L::CreateMainThread(Name),
    };

    return VMs[Name];
}

ScriptEngine::ParallelVM* ScriptEngine::CreateParallelVM()
{
    std::string name = "Parallel" + std::to_string(ParallelVMs.size());

    ParallelVM* vm = new ParallelVM;
    vm->Name = name;
    vm->MainThread = L::CreateMainThread(name);

    L::StateUserdata* vmud = (L::StateUserdata*)lua_getthreaddata(vm->MainThread);
    vmud->PVM = vm;

    lua_Callbacks* cb = lua_callbacks(vm->MainThread);

    cb->debugbreak = nullptr;
    cb->debuginterrupt = nullptr;

    ParallelVMs.push_back(vm);
    return vm;
}

lua_Type ScriptEngine::ReflectionTypeToLuauType(Reflection::ValueType rvt)
{
    if (rvt == Reflection::ValueType::Any)
    {
        assert(false);
        return LUA_TNIL;
    }

    assert(size_t(rvt & ~Reflection::ValueType::Null) < std::size(s_ValueTypeToLuauType));
    return (lua_Type)s_ValueTypeToLuauType[rvt & ~Reflection::ValueType::Null].Type;
}

static int shouldResume_Wait(
    ScriptEngine::YieldedCoroutine& CorInfo,
    lua_State* L
)
{
    if (double curTime = GetRunningTime(); curTime >= CorInfo.RmWait.ResumeAt)
    {
        lua_pushnumber(L, curTime - CorInfo.RmWait.YieldedAt);
        return 1;
    }
    else
        return -1;
}

static int shouldResume_Deferred(
    ScriptEngine::YieldedCoroutine& CorInfo,
    lua_State* L
)
{
    if (double curTime = GetRunningTime(); curTime >= CorInfo.RmDeferred.ResumeAt)
    {
        if (CorInfo.RmDeferred.Arguments)
        {
            int narg = lua_gettop(CorInfo.RmDeferred.Arguments);
            lua_xmove(CorInfo.RmDeferred.Arguments, L, narg);
            lua_unref(L, CorInfo.RmDeferred.ArgumentsRef);

            return narg;
        }
        else
            return 0;
    }
    else
        return -1;
}

static int shouldResume_Promise(ScriptEngine::YieldedCoroutine& CorInfo, lua_State* L)
{
    assert(CorInfo.RmPromise);
    const std::shared_future<std::vector<Reflection::GenericValue>>& future = CorInfo.RmPromise_Future;

    if (future.valid()
        && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready
    )
    {
        std::vector<Reflection::GenericValue> returnVals = future.get();
        for (const Reflection::GenericValue& v : returnVals)
            ScriptEngine::L::PushGenericValue(L, v);

        delete CorInfo.RmPromise;
        CorInfo.RmPromise = nullptr;
        return (int)returnVals.size();
    }
    else
        return -1;
}

static int shouldResume_Polled(ScriptEngine::YieldedCoroutine& CorInfo, lua_State* L)
{
    return CorInfo.RmPoll(L);
}

using ResumptionModeHandler = int(*)(ScriptEngine::YieldedCoroutine&, lua_State*);

static const ResumptionModeHandler s_ResumptionModeHandlers[] = {
    nullptr,

    shouldResume_Wait,
    shouldResume_Deferred,
    shouldResume_Promise,
    shouldResume_Polled,
    shouldResume_Polled,
};

static void processParallelSpawnRequests(ScriptEngine::ParallelVM* vm)
{
    ZoneScoped;

    vm->ParallelSpawnRequestsMutex.lock();
    std::vector<std::pair<std::string, std::vector<Reflection::GenericValue>>> requests = vm->ParallelSpawnRequests;
    vm->ParallelSpawnRequests.clear();
    vm->ParallelSpawnRequestsMutex.unlock();

    for (const auto& [ path, arguments ] : requests)
    {
        ZoneScopedN("process");
        ZoneText(path.data(), path.size());

        bool read = false;
        std::string contents = FileRW::ReadFile(path, &read);

        if (!read)
        {
            Log.ErrorF("Failed to read parallel script: {}", contents);
            continue;
        }

        lua_State* L = lua_newthread(vm->MainThread);
        luaL_sandboxthread(L);

        int result = ScriptEngine::CompileAndLoad(L, contents, "@" + FileRW::ResolvePathNormalized(path));

        if (result != 0)
        {
            Log.ErrorF("Failed to compile parallel script '{}': {}", path, lua_tostring(L, -1));
            lua_pop(vm->MainThread, 1);
            continue;
        }

        for (const Reflection::GenericValue& gv : arguments)
            ScriptEngine::L::PushGenericValue(L, gv);

        ZoneNamedN(resumezone, "resume", true);
        result = lua_resume(L, nullptr, (int)arguments.size());

        if (result != LUA_OK && result != LUA_YIELD && result != LUA_BREAK)
        {
            Log.ErrorF("Parallel script init: {}", lua_tostring(L, -1));
            ScriptEngine::L::DumpStacktrace(L);
        }

        lua_pop(vm->MainThread, 1);
    }
}

void ScriptEngine::LuauVM::StepScheduler(std::deque<YieldedCoroutine>* YieldedOverride)
{
    ZoneScopedC(tracy::Color::LightSkyBlue);
    ZoneText(Name.data(), Name.size());

    L::StateUserdata* vmud = (L::StateUserdata*)lua_getthreaddata(MainThread);

    if (vmud->BeingDebugged)
        return;

    std::deque<YieldedCoroutine>* yieldedCoros;
    if (YieldedOverride)
        yieldedCoros = YieldedOverride;
    else
        yieldedCoros = &YieldedCoroutines;

    for (auto it = yieldedCoros->begin(); it != yieldedCoros->end();)
    {
        if (it->Dead)
            it = yieldedCoros->erase(it);
        else
            it++;
    }

    std::deque<YieldedCoroutine*> processing;
    processing.resize(yieldedCoros->size());

    for (size_t i = 0; i < yieldedCoros->size(); i++)
        processing[i] = &(*yieldedCoros)[i];

    bool isSynchronized = !vmud->PVM || !vmud->PVM->Desynchronized;
    double stepStarted = GetRunningTime();

    for (YieldedCoroutine* yc : processing)
    {
        if (yieldedCoros->size() > 1000 && GetRunningTime() - stepStarted > 1.0)
        {
            Log.ErrorF("Scheduler is stalling! {} coroutines (throttling after 1 second)", yieldedCoros->size());
            break;
        }

        if (yc->Dead)
            continue;

        // make sure the datamodel still exists
        GameObject* dm = yc->DataModel.Referred();
        if (!dm || dm->IsDestructionPending || !dm->FindComponentByType(EntityComponent::DataModel) || lua_status(yc->Coroutine) == LUA_ERRRUN)
        {
            yc->Dead = true;
            continue;
        }

        lua_State* coroutine = yc->Coroutine;
        L::StateUserdata* corUd = (L::StateUserdata*)lua_getthreaddata(coroutine);
        assert(!corUd->BeingDebugged && "That should only be set on the main thread!");

        int corRef = yc->CoroutineReference;

        const ResumptionModeHandler handler = s_ResumptionModeHandlers[yc->Mode];
        assert(handler);

        int nretvals = handler(*yc, coroutine);

        if (nretvals >= 0)
        {
            ZoneScopedN("Resume");
            ZoneText(yc->DebugString.data(), yc->DebugString.size());

            if (lua_Debug ar = {}; lua_getinfo(coroutine, 0, "sl", &ar))
                ZoneTextF("%s:%d", ar.short_src, ar.currentline);
            else
                ZoneText(corUd->SpawnTrace.data(), corUd->SpawnTrace.size());

            yc->Dead = true;

            // TODO parallel debugger
            int resumeStatus = -1;
            if (isSynchronized)
                resumeStatus = L::Resume(coroutine, nullptr, nretvals);
            else
            {
                vmud->LastResumed = GetRunningTime();
                resumeStatus = lua_resume(coroutine, nullptr, nretvals);
            }

            if (resumeStatus != LUA_OK && resumeStatus != LUA_YIELD && resumeStatus != LUA_BREAK)
            {
                int top = lua_gettop(coroutine);
                const char* err = lua_tostring(coroutine, -1); // can't use `luaL_tolstring` because it might do a metatable check and trigger another exception

                Log.ErrorF(
                    "Script resumption: {}",
                    err ? err : "unknown error"
                );
                lua_settop(coroutine, top);

                L::DumpStacktrace(coroutine);
            }

            lua_unref(coroutine, corRef);

            if (vmud->BeingDebugged)
                break;
        }
    }
}

void ScriptEngine::ParallelVM::StepParallelScheduler(ExecutionPhase Phase)
{
    ZoneScopedC(tracy::Color::LightSkyBlue);
    ZoneText(Name.data(), Name.size());

    if (Phase == ExecutionPhase::Parallel)
    {
        /*
        L::StateUserdata* vmud = (L::StateUserdata*)lua_getthreaddata(MainThread);
        for (lua_State* coro : vmud->Coroutines)
        {
            if (lua_costatus(lua_mainthread(coro), coro) == LUA_COSUS)
            {
                lua_pushthread(coro);
                CoroutineRefs.push_back(lua_ref(coro, -1));
            }
        }
        */

        processParallelSpawnRequests(this);
        LuauVM::StepScheduler();
    }
    else
        LuauVM::StepScheduler(&YieldedCoroutinesSync);
}

static void processParallelEvents()
{
    ZoneScoped;

    std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(ScriptEngine::ParallelEventsMutex);

    for (const auto& pe : ScriptEngine::ParallelEvents)
        pe();

    ScriptEngine::ParallelEvents.clear();
}

void ScriptEngine::StepVMs()
{
    ZoneScopedC(tracy::Color::LightSkyBlue);
    CollectParallelResourceGarbage();

    for (ParallelVM* vm : ParallelVMs)
    {
        for (int ref : vm->CoroutineRefs)
            lua_unref(vm->MainThread, ref);
        vm->CoroutineRefs.clear();

        vm->Desynchronized = false;
        vm->StepParallelScheduler(ExecutionPhase::Serial);
    }

    processParallelEvents();

    for (auto& it : VMs)
        it.second.StepScheduler();
}

// Also in `EngineService.cpp`!!
#define JSON_ENCODED_DATA_TAG "__HX_EncodedData"

void ScriptEngine::L::PushJson(lua_State* L, const nlohmann::json& v)
{
    switch (v.type())
    {
    case nlohmann::json::value_t::null:
    {
        lua_pushnil(L);
        break;
    }

    case nlohmann::json::value_t::boolean:
    {
        lua_pushboolean(L, (bool)v);
        break;
    }
    case nlohmann::json::value_t::number_integer:
    {
        lua_pushinteger(L, (int)v);
        break;
    }
    case nlohmann::json::value_t::number_unsigned:
    {
        lua_pushnumber(L, (double)((uint32_t)v));
        break;
    }
    case nlohmann::json::value_t::number_float:
    {
        lua_pushnumber(L, (float)v);
        break;
    }
    case nlohmann::json::value_t::string:
    {
        const std::string& str = v;
        lua_pushlstring(L, str.data(), str.size());
        break;
    }
    case nlohmann::json::value_t::array:
    {
        lua_newtable(L);

        for (int i = 0; static_cast<size_t>(i) < v.size(); i++)
        {
            lua_pushinteger(L, i + 1);
            PushJson(L, v[i]);
            lua_settable(L, -3);
        }
        
        break;
    }
    case nlohmann::json::value_t::object:
    {
        lua_newtable(L);

        for (auto it = v.begin(); it != v.end(); ++it)
        {
            std::string key = it.key();
            const nlohmann::json& data = it.value();

            if (key == JSON_ENCODED_DATA_TAG)
            {
                const std::string& type = data["type"];
                const nlohmann::json& encoded = data["data"];

                if (type == "vector")
                {
                    lua_pop(L, 1);
                    lua_pushvector(L, encoded[0], encoded[1], encoded[2]);
                    return;
                }
                else
                    luaL_error(L, "Unknown encoded datatype '%s'", type.c_str());
            }
            else
                PushJson(L, data);

            lua_setfield(L, -2, key.c_str());
        }

        break;
    }
    default:
    {
        assert(false);
        lua_pushfstring(L, "< JSON Value : %s >", v.type_name());
    }
    }
}

#define ERROR_CONTEXTUALIZED_NVARARGS(e) { \
if (Context.size() > 0) \
    luaL_error(L, e " (serializing %s)", Context.c_str()); \
else \
    luaL_error(L, e); } \

#define ERROR_CONTEXTUALIZED(e, ...) { \
if (Context.size() > 0) \
    luaL_error(L, e " in %s", __VA_ARGS__, Context.c_str()); \
else \
    luaL_error(L, e, __VA_ARGS__); } \

nlohmann::json ScriptEngine::L::ToJson(lua_State* L, int StackIndex, std::string Context)
{
    switch (lua_type(L, StackIndex))
    {
    case LUA_TNIL:
    {
        return {};
    }
    case LUA_TBOOLEAN:
    {
        return (bool)lua_toboolean(L, StackIndex);
    }
    case LUA_TNUMBER:
    {
        double n = lua_tonumber(L, StackIndex);

        if (std::floor(n) == n)
        {
            if (n >= 0)
                return (uint32_t)n;
            else
                return (int32_t)n;
        }

        return n;
    }
    case LUA_TSTRING:
    {
        nlohmann::json str = luaL_tolstring(L, StackIndex, nullptr);
        lua_pop(L, 1);
        return str;
    }
    case LUA_TTABLE:
    {
        nlohmann::json t = nlohmann::json::object();
        int keytype = LUA_TNIL;

        luaL_checkstack(L, 5, "JSON encode");
        lua_pushvalue(L, StackIndex);
        lua_pushnil(L);

        while (lua_next(L, -2) != 0)
        {
            if (lua_type(L, -2) != keytype && keytype != LUA_TNIL)
            {
                // C++ does not specify the order of evaluation of function arguments,
                // and `luaL_tolstring` will produce side-effects (pushing string on stack)
                const char* ktname = luaL_typename(L, -2);

                ERROR_CONTEXTUALIZED(
                    "All keys must have the same type. Previous type: %s, Current type: %s ('%s')",
                    lua_typename(L, keytype), ktname, luaL_tolstring(L, -2, nullptr)
                );
            }

            if (keytype == LUA_TNIL)
            {
                keytype = lua_type(L, -2);

                if (keytype != LUA_TSTRING && keytype != LUA_TNUMBER)
                {
                    const char* ktname = luaL_typename(L, -2);

                    ERROR_CONTEXTUALIZED(
                        "Table keys expected to be string or number, got '%s' (%s)",
                        luaL_tolstring(L, -2, nullptr), ktname
                    ); // `luaL_tolstring` always pushes the string onto the stack,
                    // which is why the succeeding arguments are offset by -1
                }

                if (keytype == LUA_TNUMBER)
                    t = nlohmann::json::array();
            }

            if (lua_type(L, -2) == LUA_TNUMBER)
            {
                int index = lua_tointeger(L, -2);

                if (index == 0)
                {
                    const char* vtname = luaL_typename(L, -1);

                    ERROR_CONTEXTUALIZED(
                        "Tables cannot be zero-indexed. Value: '%s' (%s)",
                        luaL_tolstring(L, -1, nullptr), vtname
                    );
                }

                if (index < 0)
                {
                    const char* vtname = luaL_typename(L, -1);

                    ERROR_CONTEXTUALIZED(
                        "Tables cannot have negative indices. Index: %i, Value: '%s' (%s)",
                        index, luaL_tolstring(L, -1, nullptr), vtname
                    );
                }

                if (Context.size() == 0)
                    Context = "Array";

                t[index - 1] = L::ToJson(L, -1, Context + "[" + std::to_string(index) + "]");
            }
            else
            {
                assert(lua_type(L, -2) == LUA_TSTRING);
                const char* key = luaL_checkstring(L, -2);

                if (strcmp(key, JSON_ENCODED_DATA_TAG) == 0)
                    ERROR_CONTEXTUALIZED_NVARARGS("The table key '" JSON_ENCODED_DATA_TAG "' is reserved");

                if (Context.size() == 0)
                    Context = "Dictionary";

                t[key] = L::ToJson(L, -1, Context + "." + key);
            }

            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        return t;
    }

    case LUA_TVECTOR:
    {
        const float* vec = luaL_checkvector(L, StackIndex);

        nlohmann::json value;
        nlohmann::json& data = value[JSON_ENCODED_DATA_TAG];
        data["type"] = "vector";
        data["data"][0] = vec[0];
        data["data"][1] = vec[1];
        data["data"][2] = vec[2];

        return value;
    }

    [[unlikely]] default:
    {
        const char* vtname = luaL_typename(L, StackIndex);
        const char* vstr = luaL_tolstring(L, StackIndex, nullptr);

        ERROR_CONTEXTUALIZED(
            "Cannot serialize '%s' (%s) to a JSON value",
            vstr, vtname
        );
    }
    }
}
#undef ERROR_CONTEXTUALIZED

std::string ScriptEngine::CompileBytecode(const std::string_view& SourceCode, int OptimizationLevel, int DebugLevel)
{
    ZoneScoped;

    // Tell Luau that these are mutable. Otherwise, GETIMPORT optimizations
    // will cause them to be treated as constants and only invoke their `__index` functions
    // once and cache the result
    const char* mutableGlobals[] = {
        "game", "workspace",
        NULL
    };

    Luau::CompileOptions compileOptions;
    compileOptions.optimizationLevel = OptimizationLevel != -1 ? OptimizationLevel : (DeveloperTools::Initialized ? 1 : 2);
    compileOptions.debugLevel = DebugLevel != -1 ? DebugLevel : (DeveloperTools::Initialized ? 2 : 1);
    compileOptions.mutableGlobals = mutableGlobals;

    std::string bytecode = Luau::compile(std::string(SourceCode), compileOptions);
    return bytecode;
}

static constexpr std::string_view CodeGenStatuses[] = {
    "Success",
    "Nothing to compile",
    "Module not declared native, or no native functions",
    "CodeGen not initialized",
    "Overflowed instruction limit",
    "Overflowed block limit",
    "Overflowed block instruction limit",
    "Assembler finalization failed",
    "Lowering failed",
    "Allocation error",

    "<COUNT>",
};

static_assert(std::size(CodeGenStatuses) == (int)Luau::CodeGen::CodeGenCompilationResult::Count + 1);

static bool isCodeGenResultErroneous(Luau::CodeGen::CodeGenCompilationResult result)
{
    return result != Luau::CodeGen::CodeGenCompilationResult::Success
        && result != Luau::CodeGen::CodeGenCompilationResult::NothingToCompile
        && result != Luau::CodeGen::CodeGenCompilationResult::NotNativeModule;
}

#define REGISTRY_CHUNKS ("CHUNK")

static std::unordered_map<std::string, std::vector<std::pair<int, bool>>> QueuedScriptBreakpoints;

int ScriptEngine::LoadBytecode(lua_State* L, const std::string_view& Bytecode, const std::string& ChunkName)
{
    ZoneScoped;
    ZoneText(ChunkName.data(), ChunkName.size());

    int status = luau_load(L, ChunkName.c_str(), Bytecode.data(), Bytecode.size(), 0);

    if (status == 0)
    {
        if (Luau::CodeGen::isSupported())
        {
            Luau::CodeGen::CompilationOptions opts;
            opts.flags = Luau::CodeGen::CodeGenFlags::CodeGen_OnlyNativeModules;

            Luau::CodeGen::CompilationResult result = Luau::CodeGen::compile(L, -1, opts);

            if (result.hasErrors())
            {
                if (isCodeGenResultErroneous(result.result))
                    Log.ErrorF("CodeGen encountered an error when compiling {}: {}", ChunkName, CodeGenStatuses[(int)result.result]);

                for (const Luau::CodeGen::ProtoCompilationFailure& protoError : result.protoFailures)
                {
                    if (isCodeGenResultErroneous(protoError.result))
                        Log.ErrorF("CodeGen encountered an error when compiling '{}' for {}: {}", protoError.debugname, ChunkName, CodeGenStatuses[(int)protoError.result]);
                }
            }
        }

        if (ChunkName[0] == '@')
        {
            int chunk = lua_gettop(L);
            std::string path = FileRW::ResolvePathAbsolute(ChunkName.substr(1, ChunkName.size() - 1));

            if (std::filesystem::is_regular_file(path))
            {
                lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_CHUNKS);
                assert(lua_type(L, -1) == LUA_TTABLE);

                if (lua_getfield(L, -1, path.c_str()) == LUA_TNIL)
                {
                    lua_pop(L, 1);
                    lua_pushvalue(L, chunk);
                    lua_setfield(L, -2, path.c_str());
                }

                assert(lua_gettop(L) > chunk);
                lua_settop(L, chunk);

                if (const auto& it = QueuedScriptBreakpoints.find(path); it != QueuedScriptBreakpoints.end())
                {
                    for (const auto& [ line, enabled ] : it->second)
                    {
                        int actualLine = lua_breakpoint(L, chunk, line, (int)enabled);

                        if (actualLine != line)
                            EcScriptEngineService::SignalBreakpointMoved({ path, line, actualLine });
                    }

                    QueuedScriptBreakpoints.erase(it);
                }
            }
        }
    }

    return status;
}

int ScriptEngine::CompileAndLoad(lua_State* L, const std::string_view& SourceCode, const std::string& ChunkName)
{
    ZoneScoped;
    ZoneText(ChunkName.data(), ChunkName.size());

    std::string bytecode = CompileBytecode(SourceCode);
    int result = LoadBytecode(L, bytecode, ChunkName);

    return result;
}

int ScriptEngine::SetScriptBreakpoint(const std::string& VM, const std::string& Script, int Line, bool Enabled)
{
    const auto& lvmit = VMs.find(VM);
    if (lvmit == VMs.end())
        RAISE_RT("Invalid Luau VM '{}'", VM);

    LuauVM& lvm = lvmit->second;
    std::string path = FileRW::ResolvePathAbsolute(Script);
    int initial = lua_gettop(lvm.MainThread);

    lua_getfield(lvm.MainThread, LUA_REGISTRYINDEX, REGISTRY_CHUNKS);
    assert(lua_type(lvm.MainThread, -1) == LUA_TTABLE);

    int lineApplied = Line;

    if (lua_getfield(lvm.MainThread, -1, path.c_str()) == LUA_TNIL)
    {
        std::vector<std::pair<int, bool>>& queued = QueuedScriptBreakpoints[path];
        queued.emplace_back(Line, Enabled);
    }
    else
    {
        assert(lua_type(lvm.MainThread, -1) == LUA_TFUNCTION);
        lineApplied = lua_breakpoint(lvm.MainThread, -1, Line, (int)Enabled);
    }

    assert(lua_gettop(lvm.MainThread) >= initial);
    lua_settop(lvm.MainThread, initial);
    return lineApplied;
}

#define MAXTABLEDEPTH (4)

static Reflection::GenericValue toGenericValue(lua_State* L, int StackIndex, int Depth)
{
    using namespace ScriptEngine;
    using namespace ScriptEngine::L;

    switch (lua_type(L, StackIndex))
    {
    case LUA_TNIL:
    {
        return Reflection::GenericValue();
    }
    case LUA_TBOOLEAN:
    {
        return (bool)lua_toboolean(L, StackIndex);
    }
    case LUA_TNUMBER:
    {
        return lua_tonumber(L, StackIndex);
    }
    case LUA_TSTRING:
    {
        size_t len = 0;
        const char* str = lua_tolstring(L, StackIndex, &len);

        return std::string_view(str, len);
    }
    case LUA_TBUFFER:
    {
        size_t len = 0;
        void* p = lua_tobuffer(L, StackIndex, &len);

        Reflection::GenericValue val = std::string_view((char*)p, len);
        val.Type = Reflection::ValueType::Buffer;
        return val;
    }
    case LUA_TVECTOR:
    {
        return glm::make_vec3(luaL_checkvector(L, StackIndex));
    }
    case LUA_TUSERDATA:
    {
        if (const Color* col = (Color*)lua_touserdatatagged(L, StackIndex, UserdataTag::Color))
            return col->ToGenericValue();
        else if (const glm::mat4* mtx = (glm::mat4*)lua_touserdatatagged(L, StackIndex, UserdataTag::Matrix))
            return *mtx;
        else if (const uint32_t* id = (uint32_t*)lua_touserdatatagged(L, StackIndex, UserdataTag::GameObject))
        {
            Reflection::GenericValue gv = *id;
            gv.Type = Reflection::ValueType::GameObject;

            if (*id != PHX_GAMEOBJECT_NULL_ID)
            {
                // TODO GVOBJECT decremented in destructor of GenericValue
                GameObjectManager::Get()->FindById(*id)->IncrementHardRefs();
            }

            return gv;
        }
        else
            luaL_error(L, "Couldn't convert a %s to a GenericValue (unrecognized)", lua_getuserdataname(L, lua_userdatatag(L, -1)));
    }
    case LUA_TFUNCTION:
    {
        Reflection::GenericValue gv;
        gv.Type = Reflection::ValueType::Function;

        lua_State* CL = lua_newthread(L);
        int ref = lua_ref(L, -1);
        lua_pushvalue(L, StackIndex);
        lua_xmove(L, CL, 1);

        std::string fndbinfo;
        lua_Debug ar;
        lua_getinfo(L, 0, "n", &ar);
        std::string fnname = ar.name;
        if (fnname == "__namecall")
            fnname = lua_namecallatom(L, nullptr);

        lua_getinfo(L, 1, "sln", &ar);
        fndbinfo = std::format(
            "{}:{} to {} in {}",
            ar.short_src, ar.currentline, fnname, ar.name ? ar.name : "<anonymous>"
        );

        gv.Val.Func.Func = new std::function([CL, fndbinfo](const std::vector<Reflection::GenericValue>& Inputs)
            -> std::vector<Reflection::GenericValue>
            {
                lua_pushvalue(CL, -1); // keep the function value

                for (const Reflection::GenericValue& i : Inputs)
                    PushGenericValue(CL, i);

                StateUserdata* ud = (StateUserdata*)lua_getthreaddata(CL);
                ud->YieldBlockers.push_back(fndbinfo.c_str());

                int status = L::ProtectedCall(CL, (int)Inputs.size(), -1, 0);

                ud->YieldBlockers.pop_back();

                if (status == LUA_OK)
                {
                    std::vector<Reflection::GenericValue> retvals;
                    for (int i = 2; i < lua_gettop(CL); i++)
                        retvals.push_back(ToGeneric(CL, i));

                    return retvals;
                }

                RAISE_RT_NF(luaL_checkstring(CL, -1));
            });

        gv.Val.Func.Cleanup = new std::function([CL, ref]()
        {
            lua_unref(CL, ref);
        });

        lua_pop(L, 1);
        return gv;
    }
    case LUA_TTABLE:
    {
        if (Depth > MAXTABLEDEPTH)
            luaL_error(L, "Table depth limit exceeded during serialization");

        std::vector<Reflection::GenericValue> items;
        int keyType = LUA_TNONE;
        int lastIndex = 0;

        // luau.org/api/#tables
        for (int iter = 0; (iter = lua_rawiter(L, StackIndex, iter)) != -1;)
        {
            int currentKeyType = lua_type(L, -2);

            if (currentKeyType != keyType)
            {
                if (keyType == LUA_TNONE)
                    keyType = currentKeyType;
                else
                {
                    luaL_error(
                        L,
                        "Mixed tables are not allowed. Previous key type: %s, current key type: %s",
                        lua_typename(L, keyType), lua_typename(L, currentKeyType)
                    );
                }
            }

            if (keyType == LUA_TNUMBER)
            {
                double index = lua_tonumber(L, -2);

                if (index == std::floor(index))
                {
                    if ((int)index == lastIndex + 1)
                    {
                        items.push_back(toGenericValue(L, -1, Depth + 1));
                        lastIndex = (int)index;
                    }
                    else
                        luaL_error(L, "Expected consecutive array indices, got index %d with previous index %d", (int)index, lastIndex);
                }
                else
                    luaL_error(L, "Numerical indices are expected to be positive integers for arrays, got %f", index);
            }
            else
            {
                items.push_back(toGenericValue(L, -2, Depth + 1));
                items.push_back(toGenericValue(L, -1, Depth + 1));
            }

            lua_pop(L, 2);
        }

        Reflection::GenericValue value = Reflection::GenericValue(items);

        if (keyType != LUA_TNONE && keyType != LUA_TNUMBER)
            value.Type = Reflection::ValueType::Map;

        return value;
    }
    default:
    {
        const char* tname = luaL_typename(L, StackIndex);
        //luaL_error(L, "Could not convert type '%s' to a GenericValue (no conversion case)", tname);
        return tname;
    }
    }
}

Reflection::GenericValue ScriptEngine::L::ToGeneric(lua_State* L, int StackIndex)
{
    return toGenericValue(L, StackIndex, 0);
}

void ScriptEngine::L::CheckType(lua_State* L, Reflection::ValueType Type, int StackIndex)
{
    ZoneScoped;

    if (Type == Reflection::ValueType::Any)
        return; // Nothing to check

    bool isOptional = Type & Reflection::ValueType::Null;
    int givenType = lua_type(L, StackIndex);

    if (!isOptional || givenType != LUA_TNIL)
    {
        const LuauType& lty = s_ValueTypeToLuauType[Type & ~Reflection::ValueType::Null];

        // the literal `if` check inside this function likes to take 190 microseconds sometimes in Debug mode for some reason
        // probably some cache bullshit
        // fuck
        luaL_checktype(L, StackIndex, lty.Type);

        if (lty.Type == LUA_TUSERDATA)
        {
            if (lty.Tag == UserdataTag::invalid)
                luaL_typeerror(L, StackIndex, Reflection::TypeAsString(Type).c_str());
            else
                luaL_checkudatatagged(L, StackIndex, lty.Tag);
        }
        else if (lty.Type == LUA_TVECTOR && (Type & ~Reflection::ValueType::Null) == Reflection::ValueType::Vector2)
        {
            luaL_argcheck(L, luaL_checkvector(L, StackIndex)[2] == 0.f, StackIndex, "vector Z component must be 0 for a 2D vector");
        }
    }
}

void ScriptEngine::L::PushGenericValue(lua_State* L, const Reflection::GenericValue& gv)
{
    luaL_checkstack(L, 1, "::PushGenericValue");

    switch (gv.Type)
    {
    case Reflection::ValueType::Null:
    {
        lua_pushnil(L);
        break;
    }
    case Reflection::ValueType::Boolean:
    {
        lua_pushboolean(L, gv.AsBoolean());
        break;
    }
    case Reflection::ValueType::Integer:
    {
        lua_pushinteger(L, static_cast<int32_t>(gv.AsInteger()));
        break;
    }
    case Reflection::ValueType::Double:
    {
        lua_pushnumber(L, gv.AsDouble());
        break;
    }
    case Reflection::ValueType::String:
    {
        lua_pushlstring(L, gv.AsStringView().data(), gv.AsStringView().size());
        break;
    }
    case Reflection::ValueType::Buffer:
    {
        void* p = lua_newbuffer(L, gv.Size);
        memcpy(p, gv.Val.Str, gv.Size);

        break;
    }
    case Reflection::ValueType::Vector2:
    {
        luhx_pushvector3(L, glm::vec3(gv.AsVector2(), 0.f));
        break;
    }
    case Reflection::ValueType::Vector3:
    {
        luhx_pushvector3(L, gv.AsVector3());
        break;
    }
    case Reflection::ValueType::Color:
    {
        luhx_pushcolor(L, gv);
        break;
    }
    case Reflection::ValueType::Matrix:
    {
        luhx_pushmatrix(L, gv.AsMatrix());
        break;
    }
    case Reflection::ValueType::GameObject:
    {
        luhx_pushgameobject(L, GameObjectManager::Get()->FromGenericValue(gv));
        break;
    }
    case Reflection::ValueType::Array:
    {
        std::span<Reflection::GenericValue> array = gv.AsArray();
        luaL_checkstack(L, 6, "::PushGenericValue of type Array");
        lua_newtable(L);

        for (int index = 0; static_cast<size_t>(index) < array.size(); index++)
        {
            lua_pushinteger(L, index + 1);
            L::PushGenericValue(L, array[index]);
            lua_settable(L, -3);
        }

        break;
    }
    case Reflection::ValueType::Map:
    {
        // NOTE Using `::AsArray` will throw an exception because Type != Array!!
        std::span<Reflection::GenericValue> array = { gv.Val.Array, gv.Size };

        if (array.size() % 2 != 0)
            RAISE_RT("GenericValue type was Map, but it does not have an even number of elements!");

        lua_createtable(L, 0, (uint32_t)array.size() / 2);

        for (int index = 0; static_cast<size_t>(index) < array.size(); index++)
        {
            L::PushGenericValue(L, array[index]);

            if ((index + 1) % 2 == 0)
                lua_settable(L, -3);
        }

        break;
    }
    case Reflection::ValueType::EventSignal:
    {
        luhx_pushsignal(
            L,
            gv.Val.Event.Descriptor,
            gv.Val.Event.Reflector,
            gv.Val.Event.Name,
            gv.Val.Event.RestrictDataModel
        );

        break;
    }
    case Reflection::ValueType::InputEvent:
    {
        luhx_pushinputevent(L, gv.Val.Input);
        break;
    }
    case Reflection::ValueType::Function:
    {
        lua_pushliteral(L, "[function]");
        break;
    }
    default:
    {
        std::string typeName = Reflection::TypeAsString(gv.Type);
        luaL_error(L, "Cannot reflect values of type '%s'", typeName.data());
    }
    }
}

int ScriptEngine::L::HandleMethodCall(
    lua_State* L,
    const Reflection::MethodDescriptor* func,
    ReflectorRef Reflector
)
{
    const std::span<const Reflection::ValueType>& paramTypes = func->Parameters;
    int numArgs = lua_gettop(L) - 1;
    assert(numArgs >= 0);
    // missing parameter declarations?
    assert(paramTypes.size() >= static_cast<size_t>(numArgs));

    int numParams = static_cast<int32_t>(paramTypes.size());
    int minArgs = 0;

    for (Reflection::ValueType param : paramTypes)
    {
        if (!(param & Reflection::ValueType::Null))
            minArgs++;
        else
            break;
    }

    if (numArgs < minArgs)
    {
        std::string argsString = ": ( ";

        if (numArgs > 0)
        {
            for (int arg = 1; arg < numArgs + 1; arg++)
                argsString += std::string(luaL_typename(L, -(numArgs + 1 - arg))) + ", ";
            
            // trailing `, `
            argsString = argsString.substr(0, argsString.size() - 2);

            argsString += " )";
        }
        else
            argsString.clear();

        luaL_error(L,
            "Function expects at least %i arguments, got %i instead%s", 
            numParams, numArgs, argsString.c_str()
        );
    }
    else if (numArgs > minArgs)
    {
        int clip = numArgs;

        for (int arg = numArgs; arg > 0; arg--)
        {
            if (lua_isnil(L, arg + 1))
                clip = arg - 1;
            else
                break;
        }

        numArgs = clip;
    }
    else if (numArgs > numParams)
    {
        Log.WarningF(
            "Function received {} more arguments than necessary",
            numArgs - numParams
        );
    }

    assert(luaL_checkudatatagged(L, 1, UserdataTag::GameObject));
    std::vector<Reflection::GenericValue> inputs;

    for (int index = 1; index <= numArgs; index++)
    {
        Reflection::ValueType paramType = paramTypes[index - 1];

        ScriptEngine::L::CheckType(L, paramType, index + 1);
        inputs.push_back(L::ToGeneric(L, index + 1));

        if (paramType == Reflection::ValueType::Vector2)
            inputs.back().Type = Reflection::ValueType::Vector2; // no native Vector2 type, but `vector` still works fine
    }

    if (func->Yields)
    {
        return ScriptEngine::L::Yield(
            L,
            0,
            [func, inputs, Reflector](YieldedCoroutine& yc)
            {
                std::promise<std::vector<Reflection::GenericValue>>* sf = func->YieldFunction(Reflector.Referred(), inputs);
                yc.Mode = YieldedCoroutine::ResumptionMode::Promise;
                yc.RmPromise = sf;
                yc.RmPromise_Future = sf->get_future().share();
            }
        );
    }

    // Now, onto the *REAL* business...
    std::vector<Reflection::GenericValue> outputs;

    try
    {
        outputs = func->Function(Reflector.Referred(), inputs);
    }
    catch (const std::runtime_error& err)
    {
        luaL_error(L, "%s", err.what());
    }

    assert(outputs.size() == func->Returns.size());

    for (size_t i = 0; i < outputs.size(); i++)
    {
        const Reflection::GenericValue& output = outputs[i];
        assert(Reflection::TypeFits(func->Returns[i], output.Type));

        L::PushGenericValue(L, output);
    }

    StateUserdata* vmud = (StateUserdata*)lua_getthreaddata(lua_mainthread(L));
    vmud->LastResumed = GetRunningTime(); // don't count external methods like `Engine:ShowMessageBox`

    return (int)outputs.size();

    // ... kinda expected more, but ngl i feel SOOOO gigabrain for
    // giving ::GenericValue an Array, like, it all just clicks in now!
    // And then Maps just being Arrays, except odd elements are the keys
    // and even elements are the values?! Call me Einstein already on god-
    // (Me writing this as Rendering is completely busted and I have no clue
    // why oh no
    // 15/08/2024
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-aliasing"
#endif

#include <imgui_internal.h> // needed for `ImGuiContext`

#ifdef __clang__
#pragma clang diagnostic pop
#endif

int ScriptEngine::L::Yield(lua_State* L, int NumResults, std::function<void(YieldedCoroutine&)> Configure, std::deque<YieldedCoroutine>* YieldedCorosOverride)
{
    ZoneScoped;

    if (ImGuiContext* ctx = ImGui::GetCurrentContext(); ctx && ctx->CurrentWindowStack.Size > 1)
    {
        lua_Debug ar;
        lua_getinfo(L, 0, "n", &ar);
        RAISE_RT(
            "Cannot yield with '{}' while in Dear ImGui section",
            ar.name ? ar.name : "<unknown>"
        );	
    }

    lua_Debug ar = {};
    lua_getinfo(L, 1, "sln", &ar);

    StateUserdata* ud = (StateUserdata*)lua_getthreaddata(L);

    if (ud->YieldBlockers.size() > 0)
    {
        std::string blockers;

        for (size_t i = ud->YieldBlockers.size(); i != 0; i--)
        {
            blockers.append(ud->YieldBlockers[i - 1]);
            blockers.append("\n");
        }

        lua_Debug yieldar = {};
        lua_getinfo(L, 0, "n", &yieldar);

        RAISE_RT(
            "{}:{} in {}: Cannot yield right now with '{}', blocked by the following functions:\n{}",
            ar.short_src, ar.currentline, ar.name ? ar.name : "<anonymous>", yieldar.name ? yieldar.name : "<unknown>", blockers.c_str()
        );
    }

    if (!lua_isyieldable(L))
    {
        // if a `lua_Exception` is thrown by `lua_yield`, we hit an assertion in
        // `ldo.cpp` line 137
        // LUAU_ASSERT(e.getThread() == L)
        lua_Debug yieldar = {};
        lua_getinfo(L, 0, "n", &yieldar);
        RAISE_RT("Cannot yield with '{}' right now (across metamethod/C-call boundary)", ar.name ? ar.name : "<unknown>");
    }

    std::vector<SharedMutex*>& lsms = ud->PVM ? ud->PVM->LockedSharedMutexes : VMs[ud->VM].LockedSharedMutexes;

    if (lsms.size() > 0)
    {
        std::string mutexesStr;
        for (SharedMutex* sm : lsms)
        {
            sm->Mutex.unlock();
            mutexesStr.append(sm->Name + ", ");
        }

        mutexesStr = mutexesStr.substr(0, mutexesStr.size() - 2); // remove `, `
        RAISE_RT("Mutexes were left locked: {}. They have been forcefully unlocked to prevent deadlock", mutexesStr);
    }

    // TODO a kind of hack to get what datamodel we're in
    lua_getglobal(L, "game");
    Reflection::GenericValue datamodelVal = ScriptEngine::L::ToGeneric(L, -1);
    GameObject* dmObject = GameObjectManager::Get()->FromGenericValue(datamodelVal);
    assert(dmObject);
    // need to do that before `lua_yield` because of thread chicanery idk how it works

    int yieldResult = lua_yield(L, NumResults);
    lua_pushthread(L);

    lua_Debug currar = {};
    lua_getinfo(L, 0, "n", &currar);

    YieldedCoroutine yc = YieldedCoroutine{
        .DebugString = currar.name ? currar.name : "<unknown>",
        .Coroutine = L,
        .CoroutineReference = lua_ref(L, -1),
        .DataModel = dmObject,
        .Mode = YieldedCoroutine::ResumptionMode::INVALID,
    };
    L::DumpStacktrace(L, &yc.DebugString);

    Configure(yc);
    assert(yc.Mode != YieldedCoroutine::ResumptionMode::INVALID);

    if (!YieldedCorosOverride)
    {
        LuauVM* vm = nullptr;

        if (ud->PVM)
            vm = ud->PVM;
        else
            vm = &VMs.at(ud->VM);

        vm->YieldedCoroutines.push_back(yc);
    }
    else
        YieldedCorosOverride->push_back(yc);

    return yieldResult; // will probably always be -1 but just in case
}

void ScriptEngine::L::PushMethod(lua_State* L, const Reflection::MethodDescriptor* /* Method */, ReflectorRef /* Reflector */)
{
    //assert(false && "NOT IMPLEMENTED");

    // if we dont do this then comparison will not work
    // ex: `game.Close == game.Close`

    //Reflection::MethodDescriptor* methodMut =
    //lua_rawgetptagged(L, LUA_REGISTRYINDEX, const_cast<Reflection::MethodDescriptor*>(Method), LightUserdataTag::GameObjectMethod);

    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1); // remove `nil`, stack empty

        // TODO WTF is this doing??
        /*
        static_assert(sizeof(Reflector) <= sizeof(void*));
        void* data = nullptr;
        memcpy(&data, &Reflector, sizeof(Reflector));

        lua_pushlightuserdatatagged(L, const_cast<Reflection::MethodDescriptor*>(Method));
        lua_pushlightuserdata(L, data);

        lua_pushcclosure(
            L,
            [](lua_State* L)
            {
                Reflection::MethodDescriptor* fn =
                    static_cast<Reflection::MethodDescriptor*>(lua_tolightuserdata(L, lua_upvalueindex(1)));
                ReflectorRef& fc = *static_cast<ReflectorRef*>(lua_tolightuserdata(L, lua_upvalueindex(2)));

                return ScriptEngine::L::HandleMethodCall(
                    L,
                    fn,
                    fc
                );
            },

            "PushMethodThunk",
            1
        ); // stack is now just closure

        lua_pushlightuserdata(L, const_cast<Reflection::MethodDescriptor*>(Method));  // stack: closure, lud
        lua_pushvalue(L, -2);                                                 // stack: closure, lud, closure
        lua_rawsetptagged(L, LUA_REGISTERINDEX, );                                    // map closure (value) to lud (key)

        // stack is now just closure
        */
    }
    // value we fetch from `_ENVIRON` will be closure that was pushed earlier
}

// modified version of `db_traceback` from `VM/src/ldblib.cpp`
void ScriptEngine::L::DumpStacktrace(
    lua_State* L,
    std::string* Into,
    int Level,
    const char* Message
)
{
  if   (Into)
        Into->clear();

    lua_Debug ar;

    if (Message)
    {
        if (Into)
        {
            Into->append(Message);
            Into->append("\n");
        }
        else
            Log.Append(Message);
    }

    for (int i = Level; lua_getinfo(L, i, "sln", &ar); i++)
    {
        std::string line = "from ";

        if (ar.short_src)
        {
            std::string_view shortened = ar.short_src;
            if (size_t scriptsPos = shortened.find("resources/scripts/"); scriptsPos != std::string::npos)
                shortened = shortened.substr(scriptsPos + strlen("resources/"), shortened.size() - scriptsPos - strlen("resources/"));

            line.append(shortened);
        }

        if (ar.currentline > 0)
        {
            line.append(":");
            line.append(std::to_string(ar.currentline));
        }

        if (ar.name)
        {
            line.append(" in ");

            if (i == 0 && strcmp(ar.name, "__namecall") == 0)
                line.append(lua_namecallatom(L, nullptr));
            else
                line.append(ar.name);
        }

        if (!Into)
            Log.Append(line);
        else
        {
            Into->append(line);
            Into->append("\n");
        }
    }

    if (StateUserdata* corUd = (StateUserdata*)lua_getthreaddata(L); corUd && corUd->SpawnTrace.size() > 0)
    {
        if (Into)
        {
            Into->append("-- Spawning trace --\n");
            Into->append(corUd->SpawnTrace);
        }
        else
        {
            Log.AppendF("-- Spawning trace --\n{}", corUd->SpawnTrace);
        }
    }

    if (Into)
        Into->shrink_to_fit();
}

static void* l_alloc(void*, void* ptr, size_t, size_t nsize)
{
    assert(nsize <= UINT32_MAX);

    if (nsize == 0)
    {
        Memory::Free(ptr);
        return NULL;
    }
    else
        return Memory::ReAlloc(ptr, (uint32_t)nsize, Memory::Category::Luau);
}

static void initRequireConfig(luarequire_Configuration* config)
{
    config->is_require_allowed = [](lua_State*, void*, const char*)
        {
            return true;
        };
    config->reset = [](lua_State*, void* ctx, const char* chname)
        {
            // chunkname is prefixed with @
            assert(chname[0] == '@');
            ((std::filesystem::path*)ctx)->assign(FileRW::ResolvePathNormalized(chname + 1));
            return NAVIGATE_SUCCESS;
        };
    config->jump_to_alias = [](lua_State*, void*, const char*)
        {
            return NAVIGATE_NOT_FOUND;
        };
    config->to_parent = [](lua_State*, void* ctx)
        {
            std::filesystem::path* curpath = (std::filesystem::path*)ctx;

            if (curpath->has_parent_path() && *curpath != curpath->root_path())
            {
                *curpath = curpath->parent_path();
                return NAVIGATE_SUCCESS;
            }
            else
                return NAVIGATE_NOT_FOUND;
        };
    config->to_child = [](lua_State*, void* ctx, const char* name)
        {
            std::filesystem::path* curpath = (std::filesystem::path*)ctx;
            std::filesystem::path child = *curpath / name;

            if (!std::filesystem::exists(child))
            {
                std::string childCompiled = child.string() + "c"; // `.luauc`, if the path already ends in `.luau`
                std::string specCompliantModule = child.string() + ".luau";
                std::string bytecode = specCompliantModule + "c"; // `.luauc`

                bool hasChildCompiled = std::filesystem::is_regular_file(childCompiled);
                bool hasSpecCompliantModule = std::filesystem::is_regular_file(specCompliantModule);
                bool hasdDirectModuleBytecode = std::filesystem::is_regular_file(bytecode);

                if (!hasChildCompiled && !hasSpecCompliantModule && !hasdDirectModuleBytecode)
                {
                    if (!std::filesystem::is_regular_file(*curpath / "init.luau") && !std::filesystem::is_regular_file(*curpath / "init.luauc"))
                        return NAVIGATE_NOT_FOUND;
                }
                else
                {
                    if ((hasChildCompiled && hasSpecCompliantModule) || (hasSpecCompliantModule && hasdDirectModuleBytecode) || (hasChildCompiled && hasdDirectModuleBytecode))
                        return NAVIGATE_AMBIGUOUS;

                    child = hasChildCompiled ? childCompiled : (hasSpecCompliantModule ? specCompliantModule : bytecode);
                }
            }

            *curpath = child;
            return NAVIGATE_SUCCESS;
        };
    config->is_module_present = [](lua_State*, void* ctx)
        {
            std::filesystem::path* curpath = (std::filesystem::path*)ctx;
            if (std::filesystem::is_directory(*curpath))
            {
                // NOTE: :3
                if (std::filesystem::is_regular_file(curpath->string() + ".luau"))
                {
                    curpath->concat(".luau");
                    return true;
                }
                else if (std::filesystem::is_regular_file(curpath->string() + ".luauc"))
                {
                    curpath->concat(".luauc");
                    return true;
                }
            }

            return std::filesystem::is_regular_file(*curpath)
                    || std::filesystem::is_regular_file(curpath->string() + "c") // `.luauc`
                    || std::filesystem::is_regular_file(*curpath / "init.luau")
                    || std::filesystem::is_regular_file(*curpath / "init.luauc");
        };
    config->get_chunkname = [](lua_State*, void* ctx, char* buffer, size_t bufferSize, size_t* outSize)
        {
            std::filesystem::path* curpath = (std::filesystem::path*)ctx;
            std::string strpath = curpath->string();
            *outSize = strpath.size() + 1;

            if (bufferSize < strpath.size() + 1)
                return WRITE_BUFFER_TOO_SMALL;
            else
            {
                memcpy(buffer + 1, strpath.data(), strpath.size());
                buffer[0] = '@';
                return WRITE_SUCCESS;
            }
        };
    config->get_loadname = config->get_chunkname; // TODO what's a loadname
    config->get_cache_key = config->get_chunkname;
    config->get_config_status = [](lua_State*, void* ctx)
        {
            const std::filesystem::path& curpath = *(std::filesystem::path*)ctx;

            bool hasConfigScript = std::filesystem::is_regular_file(curpath / ".config.luau");
            bool hasLuauRc = std::filesystem::is_regular_file(curpath / ".luaurc");
            
            if (hasConfigScript && hasLuauRc)
                return CONFIG_AMBIGUOUS;
            if (!hasConfigScript && !hasLuauRc)
                return CONFIG_ABSENT;
            
            return hasConfigScript ? CONFIG_PRESENT_LUAU : CONFIG_PRESENT_JSON;
        };
    config->get_config = [](lua_State*, void* ctx, char* buffer, size_t bufferSize, size_t* outSize)
        {
            std::filesystem::path* curpath = (std::filesystem::path*)ctx;
            const std::string& configPath = (std::filesystem::is_regular_file(*curpath / ".config.luau")
                                        ? *curpath / ".config.luau"
                                        : *curpath / ".luaurc").string();

            bool success = true;
            std::string contents = FileRW::ReadFile(configPath, &success);
            *outSize = contents.size();

            if (bufferSize < contents.size())
                return WRITE_BUFFER_TOO_SMALL;
            
            memcpy(buffer, contents.data(), contents.size());
            return success ? WRITE_SUCCESS : WRITE_FAILURE;
        };
    config->load = [](lua_State* L, void* ctx, const char* /* path */, const char* chname, const char* ldname)
        {
            std::filesystem::path* curpath = (std::filesystem::path*)ctx;
            std::string modulePath;

            if (std::filesystem::is_regular_file(*curpath))
                modulePath = curpath->string();
            else
            {
                std::filesystem::path initSource = *curpath / "init.luau";
                std::filesystem::path initCompiled = *curpath / "init.luauc";

                if (std::filesystem::is_regular_file(initSource))
                    modulePath = initSource.string();
                else if (std::filesystem::is_regular_file(initCompiled))
                    modulePath = initCompiled.string();
                else
                    RAISE_RT("Could not find module, got path '{}'", curpath->string());
            }
            modulePath = FileRW::ResolvePathNormalized(modulePath); // replace backslashes with forward slashes on Windows

            // from `Luau/CLI/src/ReplRequirer.cpp` 13/08/2025

            // module needs to run in a new thread, isolated from the rest
            // note: we create ML on main thread so that it doesn't inherit environment of L
            lua_State* GL = lua_mainthread(L);
            lua_State* ML = lua_newthread(GL);
            lua_xmove(GL, L, 1);

            // new thread needs to have the globals sandboxed
            luaL_sandboxthread(ML);
            ScriptEngine::L::DumpStacktrace(L, &((ScriptEngine::L::StateUserdata*)lua_getthreaddata(ML))->SpawnTrace);

            bool isAotBytecode = modulePath.find(".luauc") != std::string::npos;
            std::string bytecode;

            bool readSuccess = true;
            std::string sourceCodeOrBytecode = FileRW::ReadFile(modulePath, &readSuccess);

            if (!readSuccess)
            {
                lua_pop(GL, 1);
                // `sourceCodeOrBytecode` or error message from `FileRW::ReadFile`
                luaL_error(L, "%s", sourceCodeOrBytecode.c_str());
            }

            if (isAotBytecode)
                bytecode = sourceCodeOrBytecode;
            else
                bytecode = ScriptEngine::CompileBytecode(sourceCodeOrBytecode);

            if (ScriptEngine::LoadBytecode(ML, bytecode, chname) == 0)
            {
                lua_pushstring(ML, ldname);
                lua_setglobal(ML, "_LOADNAME");

                int status = ScriptEngine::L::Resume(ML, L, 0);

                if (status == LUA_OK)
                {
                    if (lua_gettop(ML) == 0)
                        lua_pushstring(ML, "module must return a value");
                }
                else if (status == LUA_YIELD || status == LUA_BREAK)
                {
                    //lua_pop(L, 1);
                    //return -1;
                    lua_pushstring(L, "module cannot yield or break in the top level");
                }
                else if (!lua_isstring(ML, -1))
                    lua_pushstring(ML, "unknown error while running module");
            }
            
            // add ML result to L stack
            lua_xmove(ML, L, 1);
            if (lua_status(ML) != LUA_OK)
            {
                std::string trace;
                ScriptEngine::L::DumpStacktrace(L, &trace);
                trace = "\n" + trace;

                lua_pushlstring(L, trace.data(), trace.size());
                lua_concat(L, 2);
                lua_error(L);
            }

            // remove ML thread from L stack
            lua_remove(L, -2);

            // added one value to L stack: module result
            return 1;
        };

    assert(!config->get_alias);
}

lua_State* ScriptEngine::L::CreateMainThread(const std::string& VmName)
{
    ZoneScopedC(tracy::Color::LightSkyBlue);
    ZoneText(VmName.data(), VmName.size());

    lua_State* state = lua_newstate(l_alloc, nullptr);

    if (Luau::CodeGen::isSupported())
        Luau::CodeGen::create(state);

    // Load Standard Library ('print' etc)
    luaL_openlibs(state);
    // Load runtime-specific libraries
    luhx_openlibs(state);

    lua_createtable(state, 0, 8);
    lua_setfield(state, LUA_REGISTRYINDEX, REGISTRY_CHUNKS);

    std::filesystem::path* requirePath = new std::filesystem::path;
    luaopen_require(
        state,
        initRequireConfig,
        requirePath
    );

    lua_pushlightuserdatatagged(state, requirePath, LightUserdataTag::RequirerContext);
    lua_rawseti(state, LUA_ENVIRONINDEX, 67);

    for (uint8_t i = LightUserdataTag::start; i < LightUserdataTag::count; i++)
        lua_setlightuserdataname(state, i, LightUserdataTagNames[i].data());

    GameObjectManager* ObjectManager = GameObjectManager::Get();

    luhx_pushgameobject(state, ObjectManager->FindById(ObjectManager->DataModel));
    lua_setglobal(state, "game");

    luhx_pushgameobject(state, ObjectManager->FindById(ObjectManager->DataModel)->FindChild("Workspace"));
    lua_setglobal(state, "workspace");

    lua_pushlstring(state, VmName.data(), VmName.size());
    lua_setglobal(state, "_VMNAME");

    StateUserdata* vmud = new StateUserdata;
    lua_Callbacks* cb = lua_callbacks(state);

    if (DeveloperTools::Initialized)
    {
        cb->debugbreak = [](lua_State* L, lua_Debug* ar)
            {
                StateUserdata* vmud = (StateUserdata*)lua_getthreaddata(lua_mainthread(L));
                if (vmud->DebuggerAttached)
                {
                    if (lua_isyieldable(L))
                        DeveloperTools::OnDebugBreak(L, ar, DebugBreakReason::Breakpoint);
                    else
                    {
                        lua_getinfo(L, 0, "s", ar);

                        Log.ErrorF(
                            "Breakpoint {}:{} cannot be hit as it is not within a yieldable context (metamethod/C-call boundary)",
                            ar->short_src, ar->currentline
                        );
                    }
                }
            };
        cb->debuginterrupt = [](lua_State* L, lua_Debug* ar)
            {
                StateUserdata* vmud = (StateUserdata*)lua_getthreaddata(lua_mainthread(L));
                if (vmud->DebuggerAttached)
                    DeveloperTools::OnDebugBreak(L, ar, DebugBreakReason::Interrupt);
            };

        vmud->DebuggerAttached = true;
    }

    cb->userthread = [](lua_State* LP, lua_State* L)
        {
            if (LP)
            {
                StateUserdata* ud = new StateUserdata;
                DumpStacktrace(LP, &ud->SpawnTrace);
                lua_setthreaddata(L, ud);

                StateUserdata* vmud = (StateUserdata*)lua_getthreaddata(lua_mainthread(L));
                vmud->Coroutines.push_back(L);
                ud->PVM = vmud->PVM;
                ud->VM = vmud->VM;
            }
            else
            {
                StateUserdata* vmud = (StateUserdata*)lua_getthreaddata(lua_mainthread(L));
                vmud->Coroutines.erase(std::find(vmud->Coroutines.begin(), vmud->Coroutines.end(), L));

                StateUserdata* ud = (StateUserdata*)lua_getthreaddata(L);
                for (EventConnectionData* ec : ud->EventConnections)
                {
                    assert(ec->ConnectionId != UINT32_MAX);

                    if (void* referred = ec->Reflector.Referred())
                        ec->Event->Disconnect(referred, ec->ConnectionId);
                }

                ud->EventConnections.clear();
                delete (StateUserdata*)lua_getthreaddata(L);
            }
        };

    cb->interrupt = [](lua_State* L, int GcState)
        {
            StateUserdata* vmud = (StateUserdata*)lua_getthreaddata(lua_mainthread(L));

            if (vmud->AllowedExecutionTime != 0.f && GetRunningTime() - vmud->LastResumed > vmud->AllowedExecutionTime)
            {
                vmud->LastResumed = GetRunningTime() + 0.5; // interrupt may recurse due to GC
                luaL_error(L, "Script VM was timed-out for running for more than %lf seconds without yielding (GC: %i)", vmud->AllowedExecutionTime, GcState);
            }
        };

    vmud->AllowedExecutionTime = DefaultVMAllowedExecutionTime;
    vmud->LastResumed = GetRunningTime();
    vmud->VM = VmName;
    lua_setthreaddata(state, vmud);

    luaL_sandbox(state);
    return state;
}

void ScriptEngine::LuauVM::Close()
{
    lua_State* L = MainThread;

    lua_rawgeti(L, LUA_ENVIRONINDEX, 67);

    if (std::filesystem::path* path = (std::filesystem::path*)lua_tolightuserdatatagged(L, -1, LightUserdataTag::RequirerContext))
        delete path;
    else
        Log.Error("Could not free requirer context data");

    for (auto it = YieldedCoroutines.begin(); it != YieldedCoroutines.end(); it++)
    {
        if (!it->Dead && lua_mainthread(it->Coroutine) == L)
            it->Dead = true;
    }

    L::StateUserdata* vmud = (L::StateUserdata*)lua_getthreaddata(L);

    if (vmud->BeingDebugged)
        DeveloperTools::LeaveDebugger();

    for (lua_State* co : vmud->Coroutines)
    {
        L::StateUserdata* ud = (L::StateUserdata*)lua_getthreaddata(co);
        while (!ud->EventConnections.empty())
        {
            EventConnectionData* ec = ud->EventConnections.back();
            assert(ec->ConnectionId != UINT32_MAX);

            void* referred = ec->Reflector.Referred();
            ec->Event->Disconnect(referred, ec->ConnectionId);
        }

        ud->EventConnections.clear();
    }

    lua_close(L);
    delete vmud; // delete after closing VM due to `userthread` callback

    VMs.erase(Name);
}

static void breakHere(lua_State* L, DebugBreakReason Reason)
{
    using namespace ScriptEngine;
    ScriptEngine::L::StateUserdata* vmud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(L));

    if (vmud->DebuggerAttached)
    {
        lua_Debug ar = {};
        lua_getinfo(L, 1, "sln", &ar);
        DeveloperTools::OnDebugBreak(L, &ar, Reason);
        vmud->BeingDebugged = true;
    }
}

static lua_Status finishCoroutine(lua_State* L, int status)
{
    using namespace ScriptEngine;
    using namespace L;
    ScriptEngine::L::StateUserdata* vmud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(L));

    if (!vmud->PVM && !vmud->BeingDebugged)
    {
        if (status == LUA_BREAK)
            breakHere(L, DebugBreakReason::BrokeIntoDebugger);
        else if (status == LUA_ERRRUN)
            breakHere(L, DebugBreakReason::Error);
    }

    if (status == LUA_YIELD || status == LUA_OK)
    {
        while (!vmud->UnfinishedProfilerZones.empty())
        {
            const std::string_view& name = vmud->UnfinishedProfilerZones.top();
            Log.WarningF("Profiler zone '{}' was not finished, did you forget to call `debug.zoneend()`?", name);

            tracy::LuauZoneEndImpl();
            vmud->UnfinishedProfilerZones.pop();
        }
    }

    return (lua_Status)status;
}

lua_Status ScriptEngine::L::Resume(lua_State* L, lua_State* from, int narg)
{
    std::string extTags;

    lua_Debug ar = {};
    if (int r = lua_getinfo(L, 1, "sl", &ar); r && ar.short_src)
        extTags = std::format("Script:{},Line:{}", ar.short_src, ar.currentline);

    Logging::ScopedContext sc = Logging::ScopedContext(Logging::Context{ .ContextExtraTags = extTags });

    StateUserdata* vmud = (StateUserdata*)lua_getthreaddata(lua_mainthread(L));
    vmud->LastResumed = GetRunningTime();

    int status = lua_resume(L, from, narg);
    return finishCoroutine(L, status);
}

lua_Status ScriptEngine::L::ProtectedCall(lua_State* L, int narg, int nret, int errfunc)
{
    StateUserdata* vmud = (StateUserdata*)lua_getthreaddata(lua_mainthread(L));
    vmud->LastResumed = GetRunningTime();

    int status = lua_pcall(L, narg, nret, errfunc);
    return finishCoroutine(L, status);
}

nlohmann::json ScriptEngine::DumpApiToJson()
{
    ObjectHandle tempdm = GameObjectManager::s_Create("DataModel");
    ObjectHandle tempwp = GameObjectManager::s_Create("Workspace");
    tempwp->SetParent(tempdm);
    GameObjectManager::Get()->DataModel = tempdm->ObjectId;

    lua_State* base = lua_newstate(l_alloc, nullptr);
    // don't have to worry about re-allocs here
    LuauVM& luhxVM = RegisterNewVM("ApiDump");
    lua_State* luhx = luhxVM.MainThread;
    // Load Standard Library ('print' etc)
    luaL_openlibs(base);
    lua_pushinteger(base, 0);
    lua_setglobal(base, "require");

    // Compare the environment of our extended environment ("Luhx")
    // with the standard Luau environment to figure out what got added
    nlohmann::json json;

    lua_getglobal(luhx, "_G");
    lua_pushnil(luhx);
    while (lua_next(luhx, -2))
    {
        if (lua_islightuserdata(luhx, -1))
        {
            // 67
            lua_pop(luhx, 1);
            continue;
        }

        bool skip = false;
        std::vector<std::string> librarySpecificMembers;

        std::string k = luaL_checkstring(luhx, -2);
        if (int ty = lua_getglobal(base, k.c_str()); ty != LUA_TNIL)
        {
            skip = true;

            if (ty == LUA_TTABLE && k != "_G")
            {
                lua_pushnil(luhx);
                while (lua_next(luhx, -2))
                {
                    if (lua_type(luhx, -2) != LUA_TSTRING)
                    {
                        lua_pop(luhx, 1);
                        continue;
                    }

                    int vty = lua_getfield(base, -1, lua_tostring(luhx, -2));
                    if (vty == LUA_TNIL)
                    {
                        skip = false;
                        librarySpecificMembers.push_back(lua_tostring(luhx, -2));
                    }
                    lua_pop(base, 1);

                    lua_pop(luhx, 1);
                }
            }
        }

        if (!skip)
        {
            if (!lua_istable(luhx, -1))
            {
                const char* tn = luaL_typename(luhx, -1);

                if (strcmp(tn, "GameObject") == 0)
                {
                    std::string type = tn;

                    GameObject* obj = GameObjectManager::Get()->FromGenericValue(L::ToGeneric(luhx, -1));

                    for (const ReflectorRef& ref : obj->Components)
                    {
                        type.append(" & Ec");
                        type.append(s_EntityComponentNames[(uint8_t)ref.Type]);
                    }

                    json["Globals"][k] = type;
                }
                else
                    json["Globals"][k] = tn;
            }
            else
            {
                nlohmann::json lib;

                lua_pushnil(luhx);
                while (lua_next(luhx, -2))
                {
                    if ((librarySpecificMembers.size() > 0
                            && std::find(librarySpecificMembers.begin(), librarySpecificMembers.end(), lua_tostring(luhx, -2)) != librarySpecificMembers.end()
                        ) || librarySpecificMembers.size() == 0
                    )
                    {
                        if (k == "Enum")
                        {
                            nlohmann::json enu;

                            lua_pushnil(luhx);
                            while (lua_next(luhx, -2))
                            {
                                enu[luaL_checkstring(luhx, -2)] = lua_tointeger(luhx, -1);
                                lua_pop(luhx, 1);
                            }

                            lib[luaL_checkstring(luhx, -2)] = enu;
                        }
                        else
                        {
                            lib[luaL_checkstring(luhx, -2)] = luaL_typename(luhx, -1);
                        }
                    }

                    lua_pop(luhx, 1);
                }

                if (const auto& tagIt = std::find(std::begin(UserdataTagNames), std::end(UserdataTagNames), k); tagIt != std::end(UserdataTagNames))
                {
                    json["Datatypes"][k]["Library"] = lib;

                    lua_getuserdatametatable(luhx, (int)std::distance(UserdataTagNames, tagIt));

                    if (!lua_isnil(luhx, -1))
                    {
                        lua_pushnil(luhx);
                        while (lua_next(luhx, -2))
                        {
                            json["Datatypes"][k]["Metatable"][luaL_checkstring(luhx, -2)] = luaL_typename(luhx, -1);
                            lua_pop(luhx, 1);
                        }
                    }

                    lua_pop(luhx, 1);
                }
                else
                    json["Libraries"][k] = lib;
            }
        }

        lua_pop(base, 1);
        lua_pop(luhx, 1);
    }

    for (int dei = UserdataTag::start; dei < UserdataTag::count; dei++)
    {
        const std::string_view& name = UserdataTagNames[dei];
        if (json["Datatypes"].find(name) == json["Datatypes"].end())
            json["Datatypes"][name] = nlohmann::json::object();
    }

    lua_close(base);
    luhxVM.Close();

    GameObjectManager::Get()->DataModel = PHX_GAMEOBJECT_NULL_ID;
    tempdm->Destroy();

    return json;
}
