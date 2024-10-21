// 21/09/2024

#pragma once

#include<unordered_map>
#include<future>
#include<thread>
#include<luau/Compiler/include/luacode.h>

#include"Reflection.hpp"
#include"datatype/GameObject.hpp"

namespace ScriptEngine
{
	extern bool s_BackendScriptWantGrabMouse;

	// The `lua_State*` should be `lua_resume`'d upon the `std::shared_future` enreadying
	// (is that a real word)
	// (I mean upon it becoming ready)
	// 22/09/2024
	// I think this is possible?
	extern std::unordered_map<lua_State*, std::shared_future<Reflection::GenericValue>> s_YieldedCoroutines;

	extern const std::unordered_map<Reflection::ValueType, lua_Type> ReflectedTypeLuaEquivalent;
};

namespace ScriptEngine::L
{
	Reflection::GenericValue LuaValueToGeneric(
		lua_State*,
		int StackIndex = -1
	);

	void PushGenericValue(lua_State*, Reflection::GenericValue&);
	void PushGameObject(lua_State*, GameObject*);
	void PushFunction(lua_State* L, const char*);

	extern std::unordered_map<std::string, lua_CFunction> GlobalFunctions;
};
