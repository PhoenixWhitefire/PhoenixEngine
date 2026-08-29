// 21/09/2024

#pragma once

#include <unordered_map>
#include <cstdint>
#include <future>
#include <deque>
#include <stack>
#include <lua.h>

#include "datatype/GameObject.hpp"

#define ROOT_LVM_NAME "RootLVM"

#ifdef Yield
#undef Yield
#endif

struct EventConnectionData;
struct SharedMutex;

class ScriptEngine
{
public:
    void Initialize();
    void Shutdown();

    static ScriptEngine* Get();

    static std::string CompileBytecode(const std::string_view&, int OptimizationLevel = -1, int DebugLevel = -1);
    static int LoadBytecode(lua_State*, const std::string_view& Bytecode, const std::string& ChunkName);
    static int CompileAndLoad(lua_State*, const std::string_view& SourceCode, const std::string& ChunkName);
    static lua_Type ReflectionTypeToLuauType(Reflection::ValueType);
    nlohmann::json DumpApiToJson();

    int SetScriptBreakpoint(
        const std::string& VM,
        const std::string& File,
        int Line,
        bool Enabled
    );

    struct YieldedCoroutine
    {
        struct ResumptionMode_
        {
            enum RM : uint8_t {
                INVALID = 0,

                Wait, // resume yielded thread at a specific time (`task.wait`)
                Deferred, // resume arbitrary thread at a specific time and pass arguments
                Promise, // promise
                Polled, // poll a function,
                DeferredEventResumption // poll a function
            };
        };

        using ResumptionMode = ResumptionMode_::RM;

        std::string DebugString;

        lua_State* Coroutine = nullptr;
        int CoroutineReference = INT32_MAX;
        ObjectRef DataModel = {};

        union {
            struct {
                double YieldedAt = 0.0;
                double ResumeAt = 0.0;
            } RmWait;
            struct {
                double ResumeAt = 0.0;
                lua_State* Arguments = nullptr;
                int ArgumentsRef = 0;
            } RmDeferred;
            struct {
                const Reflection::EventDescriptor* Event = nullptr;
                ReflectorRef Reflector = {};
                uint32_t ConnectionId = UINT32_MAX;
                uint32_t RestrictDataModel = UINT32_MAX;
            } RmEventCallback = {};
        };

        std::promise<std::vector<Reflection::GenericValue>>* RmPromise = nullptr;
        std::shared_future<std::vector<Reflection::GenericValue>> RmPromise_Future = {};
        std::function<int(lua_State*)> RmPoll = {};

        ResumptionMode Mode = ResumptionMode::INVALID;
        bool Dead = false;
    };

    enum class ExecutionPhase : uint8_t
    {
        Serial,
        Parallel,
    };

    struct LuauVM
    {
        lua_State* CreateMainThread();
        void StepScheduler(std::deque<YieldedCoroutine>* Yielded = nullptr);
        void Close();

        // This yields the given Luau thread (ensuring that we are in a yieldable context),
        // and calls `Configure` to set the resumption mode and do any final preparations.
        // Like `lua_yield`, returns `-1`.
        int Yield(
            lua_State* L,
            int NumResults,
            std::function<void(YieldedCoroutine&)> Configure,
            std::deque<YieldedCoroutine>* YieldedCorosOverride = nullptr
        );

        std::deque<YieldedCoroutine> YieldedCoroutines = {};
        std::vector<SharedMutex*> LockedSharedMutexes = {};
        std::string Name;
        lua_State* MainThread = nullptr;
    };

    struct ParallelVM : public LuauVM
    {
        void StepParallelScheduler(ExecutionPhase Phase);

        std::deque<YieldedCoroutine> YieldedCoroutinesSync;
        std::vector<int> CoroutineRefs; // have to pin coroutines outside of serial phase

        std::vector<std::pair<std::string, std::vector<Reflection::GenericValue>>> ParallelSpawnRequests;
        std::mutex ParallelSpawnRequestsMutex;
        int ParallelAllocated = 0;
        bool IsParallel = false;
        bool Desynchronized = false;
    private:
        void m_ProcessParallelSpawnRequests();
    };

    void StepVMs();

    LuauVM& RegisterNewVM(const std::string& Name);
    ParallelVM* CreateParallelVM();

    std::unordered_map<std::string, LuauVM*> VMs;
    std::vector<ParallelVM*> ParallelVMs;
    std::atomic_int ParallelVMsExecuting = 0;

    std::vector<std::function<void()>> ParallelEvents;
    std::mutex ParallelEventsMutex;

    double DefaultVMAllowedExecutionTime = 0.0;

    static lua_Status Resume(lua_State* L, lua_State* from, int narg);
    static lua_Status ProtectedCall(lua_State* L, int narg, int nret, int errfunc);

    static Reflection::GenericValue ToGeneric(
        lua_State*,
        int StackIndex = -1
    );
    static nlohmann::json ToJson(lua_State*, int StackIndex = -1, std::string Context = "");
    static void CheckType(
        lua_State*,
        Reflection::ValueType,
        int StackIndex = -1
    );

    static void PushGenericValue(lua_State*, const Reflection::GenericValue&);
    static void PushJson(lua_State*, const nlohmann::json&);

    static void DumpStacktrace(lua_State* L, std::string* Into = nullptr, int Level = 0, const char* Message = nullptr);

    static int HandleMethodCall(
        lua_State* L,
        const Reflection::MethodDescriptor* fnaf, // THE MIMICCCCC!!!!
        ReflectorRef
    );

    // This yields the given Luau thread (ensuring that we are in a yieldable context),
    // and calls `Configure` to set the resumption mode and do any final preparations.
    // Like `lua_yield`, returns `-1`.
    static int Yield(
        lua_State*,
        int NumResults,
        std::function<void(YieldedCoroutine&)> Configure,
        std::deque<YieldedCoroutine>* YieldedCorosOverride = nullptr
    );

    static bool IsSynchronized(lua_State*);

    struct StateUserdata
    {
        ScriptEngine::LuauVM* VM = nullptr;
        ScriptEngine::ParallelVM* PVM = nullptr;
        std::string SpawnTrace;
        std::vector<EventConnectionData*> EventConnections;
        std::vector<lua_State*> Coroutines; // Only populated for the main thread
        std::vector<std::string> YieldBlockers;
        std::stack<std::string> UnfinishedProfilerZones;
        double AllowedExecutionTime = 0.0;
        double LastResumed = 0.0;
        bool DebuggerAttached = false;
        bool BeingDebugged = false;
    };
private:
    void m_ProcessParallelEvents();
    static void s_InitRequireConfig(struct luarequire_Configuration*);
};
