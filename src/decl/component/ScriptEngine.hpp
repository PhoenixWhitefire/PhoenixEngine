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

	// The `lua_State*` should be `lua_resume`'d upon the `std::shared_future` enreadying
	// (is that a real word)
	// (I mean upon it becoming ready)
	// 22/09/2024
	// I think this is possible?
	struct YieldedCoroutine
	{
		lua_State* Coroutine{};
		int CoroutineReference{};
		std::shared_future<Reflection::GenericValue> Future;
		uint32_t ScriptId = PHX_GAMEOBJECT_NULL_ID;
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
	void PushFunction(lua_State* L, const char*);

	int HandleFunctionCall(
		lua_State* L,
		GameObject* g,
		const char* fnaf,
		int nargs
	);

	extern std::unordered_map<std::string_view, lua_CFunction> GlobalFunctions;
};
