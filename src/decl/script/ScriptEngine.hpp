// 21/09/2024

#pragma once

#include <unordered_map>
#include <future>
#include <thread>
#include <luau/Compiler/include/luacode.h>
#include <lua.h>

#include "Reflection.hpp"
#include "datatype/GameObject.hpp"

#define LUA_ASSERT(res, err, ...) { if (!(res)) { luaL_errorL(L, err, __VA_ARGS__); } }

namespace ScriptEngine
{
	void Initialize();

	int CompileAndLoad(lua_State*, const std::string& SourceCode, const std::string& ChunkName);
	nlohmann::json DumpApiToJson();
	lua_Type ReflectionTypeToLuauType(Reflection::ValueType);
	void StepScheduler();

	struct YieldedCoroutine
	{
		struct ResumptionMode_
		{
			enum RM : uint8_t {
				INVALID = 0,

				Wait, // resume yielded thread at a specific time (`task.wait`)
				Deferred, // resume arbitrary thread at a specific time and pass arguments
				Future, // check the status of an `std::shared_future`
				Polled // poll a function
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
		};

		std::shared_future<std::vector<Reflection::GenericValue>> RmFuture;
		std::function<int(lua_State*)> RmPoll;

		ResumptionMode Mode = ResumptionMode::INVALID;
	};

	inline std::vector<YieldedCoroutine> s_YieldedCoroutines;

	struct LuauVM
	{
		std::string Name;
		lua_State* MainThread = nullptr;
		std::vector<lua_State*> Coroutines;
	};

	const LuauVM& GetCurrentVM();
	const LuauVM& RegisterNewVM(const std::string& Name);

	inline std::unordered_map<std::string, LuauVM> VMs;
	inline std::string CurrentVM;
};

namespace ScriptEngine::L
{
	lua_State* Create(const std::string& VmName);
	void Close(lua_State*);

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
		std::function<void(YieldedCoroutine&)> Configure
	);

	struct StateUserdata
	{
		std::string VM;
		std::string SpawnTrace;
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
	inline void(*LeaveDebugger)() = nullptr;
};
