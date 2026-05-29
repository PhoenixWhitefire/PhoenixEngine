// 21/09/2024

#pragma once

#include <unordered_map>
#include <cstdint>
#include <future>
#include <thread>
#include <deque>
#include <lua.h>

#include "datatype/GameObject.hpp"

#define ROOT_LVM_NAME "RootLVM"

struct EventConnectionData;
struct SharedMutex;

namespace ScriptEngine
{
	void Initialize();
	void Shutdown();

	std::string CompileBytecode(const std::string_view&, int OptimizationLevel = -1, int DebugLevel = -1);
	int LoadBytecode(lua_State*, const std::string_view& Bytecode, const std::string& ChunkName);
	int CompileAndLoad(lua_State*, const std::string_view& SourceCode, const std::string& ChunkName);
	nlohmann::json DumpApiToJson();
	lua_Type ReflectionTypeToLuauType(Reflection::ValueType);

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
		ObjectRef DataModel;

		union {
			struct {
				double YieldedAt = 0.f;
				double ResumeAt = 0.f;
			} RmWait;
			struct {
				double ResumeAt = 0.f;
				lua_State* Arguments = nullptr;
				int ArgumentsRef = 0;
			} RmDeferred;
			struct {
				const Reflection::EventDescriptor* Event = nullptr;
				ReflectorRef Reflector;
				uint32_t ConnectionId = UINT32_MAX;
			} RmEventCallback;
		};

		std::promise<std::vector<Reflection::GenericValue>>* RmPromise;
		std::shared_future<std::vector<Reflection::GenericValue>> RmPromise_Future;
		std::function<int(lua_State*)> RmPoll;

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
        void StepScheduler(std::deque<YieldedCoroutine>* Yielded = nullptr);
        void Close();

        std::deque<YieldedCoroutine> YieldedCoroutines;
        std::vector<SharedMutex*> LockedSharedMutexes;
        std::string Name;
        lua_State* MainThread = nullptr;
		double AllowedExecutionTime = 10.0;
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
    };

    void StepVMs();

    LuauVM& RegisterNewVM(const std::string& Name);
    ParallelVM* CreateParallelVM();

    inline std::unordered_map<std::string, LuauVM> VMs;
    inline std::vector<ParallelVM*> ParallelVMs;
    inline std::atomic_int ParallelVMsExecuting = 0;

	inline std::vector<std::function<void()>> ParallelEvents;
	inline std::mutex ParallelEventsMutex;

	inline double DefaultVMAllowedExecutionTime = 10.0;
};

namespace ScriptEngine::L
{
	lua_State* CreateMainThread(const std::string& VmName);

	lua_Status Resume(lua_State* L, lua_State* from, int narg);
	lua_Status ProtectedCall(lua_State* L, int narg, int nret, int errfunc);
	
	Reflection::GenericValue ToGeneric(
		lua_State*,
		int StackIndex = -1
	);
	nlohmann::json ToJson(lua_State*, int StackIndex = -1, std::string Context = "");
	void CheckType(
		lua_State*,
		Reflection::ValueType,
		int StackIndex = -1
	);

	void PushGenericValue(lua_State*, const Reflection::GenericValue&);
	void PushJson(lua_State*, const nlohmann::json&);
	void PushMethod(lua_State* L, const Reflection::MethodDescriptor*, ReflectorRef);

	void DumpStacktrace(lua_State* L, std::string* Into = nullptr, int Level = 0, const char* Message = nullptr);

	int HandleMethodCall(
		lua_State* L,
		const Reflection::MethodDescriptor* fnaf, // THE MIMICCCCC!!!!
		ReflectorRef
	);

	// This yields the given Luau thread (ensuring that we are in a yieldable context),
	// and calls `Configure` to set the resumption mode and do any final preparations.
	// Like `lua_yield`, returns `-1`.
	int Yield(
		lua_State*,
		int NumResults,
		std::function<void(YieldedCoroutine&)> Configure,
		std::deque<YieldedCoroutine>* YieldedCorosOverride = nullptr
	);

    bool IsSynchronized(lua_State*);

	struct StateUserdata
	{
		std::string VM;
		std::string SpawnTrace;
		std::vector<EventConnectionData*> EventConnections;
		std::vector<lua_State*> Coroutines; // Only populated for the main thread
		std::vector<std::string> YieldBlockers;
		double LastResumed = 0.f;
        ParallelVM* PVM = nullptr;
	};

	struct DebugBreakReason_
	{
		enum DBR {
			BrokeIntoDebugger, // `lua_break` or generic reason for thread entering `LUA_BREAK` state
			Breakpoint,        // `lua_breakpoint`
			Interrupt,         // `interruptThread` of `coresumey` in `coroutine.resume` implementation
			Error,             // error message expected to be at the top of the stack
			DebuggerStep       // debugger is stepping through the code
		};
	};
	using DebugBreakReason = DebugBreakReason_::DBR;

	inline void(*DebugBreak)(lua_State*, lua_Debug*, DebugBreakReason) = nullptr;
	inline void(*LeaveDebugger)(lua_State*) = nullptr;
};
