// 21/09/2024

#pragma once

#include <unordered_map>
#include <future>
#include <thread>
#include <luau/Compiler/include/luacode.h>

#include "Reflection.hpp"
#include "datatype/GameObject.hpp"

#define LUA_ASSERT(res, err, ...) { if (!(res)) { luaL_errorL(L, err, __VA_ARGS__); } }

namespace ScriptEngine
{
	int CompileAndLoad(lua_State*, const std::string& SourceCode, const std::string& ChunkName);
	nlohmann::json DumpApiToJson();
	
	struct YieldedCoroutine
	{
		enum class ResumptionMode : uint8_t
		{
			INVALID = 0,
			
			ScheduledTime, // resume at a specific time
			Future, // check the status of an `std::shared_future`
			Polled // poll a function
		};

		lua_State* Coroutine{};
		int CoroutineReference{};
		uint32_t ScriptId = PHX_GAMEOBJECT_NULL_ID;

		ResumptionMode Mode = ResumptionMode::INVALID;

		struct
		{
			double YieldedAt = 0.f;
			double ResumeAt = 0.f;
		} RmSchedule;
		std::shared_future<std::vector<Reflection::GenericValue>> RmFuture;
		std::function<int(lua_State*)> RmPoll;
	};

	extern std::vector<YieldedCoroutine> s_YieldedCoroutines;

	extern const std::unordered_map<Reflection::ValueType, lua_Type> ValueTypeToLuauType;
};

namespace ScriptEngine::L
{
	lua_State* Create();
	void Close(lua_State*);
	
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
	void PushGameObject(lua_State*, GameObject*);
	void PushMethod(lua_State* L, const Reflection::MethodDescriptor*, ReflectorRef);

	void DumpStacktrace(lua_State* L, std::string* Into = nullptr, int Level = 0, const char* Message = nullptr);

	int HandleMethodCall(
		lua_State* L,
		const Reflection::MethodDescriptor* fnaf, // THE MIMICCCCC!!!!
		ReflectorRef
	);

	// This yields the given Luau thread (ensuring that we are in a yieldable context),
	// and calls `Configure` to set the resumption mode and do any final preparations
	void Yield(
		lua_State*,
		int NumResults,
		std::function<void(YieldedCoroutine&)> Configure
	);

	struct GlobalFn
	{
		lua_CFunction Function;
		int NumMinArgs = 0;
	};

	// TODO replace with `std::span` once engine is moved over to C++ 26
	// (initializer lists cannot be used for spans before that)
	extern std::pair<std::string_view, GlobalFn>* GlobalFunctions;

	inline void(*DebugBreak)(lua_State*, lua_Debug*, bool) = nullptr;
};
