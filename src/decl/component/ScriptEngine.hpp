// 21/09/2024

#pragma once

#include <unordered_map>
#include <future>
#include <thread>
#include <luau/Compiler/include/luacode.h>

#include "Reflection.hpp"
#include "datatype/GameObject.hpp"

namespace ScriptEngine
{
	int CompileAndLoad(lua_State*, const std::string& SourceCode, const std::string& ChunkName);

	extern bool s_BackendScriptWantGrabMouse;

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
		std::function<bool(std::vector<Reflection::GenericValue>*)> RmPoll;
	};

	extern std::vector<YieldedCoroutine> s_YieldedCoroutines;

	extern const std::unordered_map<Reflection::ValueType, lua_Type> ReflectedTypeLuaEquivalent;
};

namespace ScriptEngine::L
{
	Reflection::GenericValue LuaValueToGeneric(
		lua_State*,
		int StackIndex = -1
	);

	void PushGenericValue(lua_State*, const Reflection::GenericValue&);
	void PushGameObject(lua_State*, GameObject*);
	void PushFunction(lua_State* L, Reflection::Function*);

	int HandleMethodCall(
		lua_State* L,
		Reflection::Function* fnaf
	);

	extern std::unordered_map<std::string_view, lua_CFunction> GlobalFunctions;
};
