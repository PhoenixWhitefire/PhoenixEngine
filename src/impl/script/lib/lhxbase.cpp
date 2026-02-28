#include <luau/VM/src/lstate.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

static std::string getScriptTraceExtraTags(lua_State* L)
{
	lua_Debug ar = {};
	lua_getinfo(L, 1, "sl", &ar);

	return std::format("TextDocument:{},DocumentLine:{}", ar.short_src , ar.currentline);
}

static void appendToLog(lua_State* L)
{
	// FROM:
	// `luaB_print`
	// `Luau/VM/src/lbaselib.cpp`
	// 11/11/2024

	int n = lua_gettop(L); // number of arguments
	for (int i = 1; i <= n; i++)
	{
		size_t l;
		const char* s = luaL_tolstring(L, i, &l); // convert to string using __tostring et al
		// Always pushes the string onto the stack

		if (i > 1)
			Log.Append(" &&");
		else
			Log.Append("&&");

		Reflection::GenericValue value;

		try
		{
			value = ScriptEngine::L::ToGeneric(L, i);
		}
		catch (const std::exception& e)
		{
			value = std::format("Failed to convert to GenericValue: {}", e.what());
		}

		if (i < n)
			Log.AppendWithValue(std::string(s) + "&&", value);
		else
			Log.AppendWithValue(s, value);

		lua_pop(L, 1); // pop result
	}
}

static int base_print(lua_State* L)
{
	// this is just so that the Output can attribute the log message to a script
	Log.Info("&&", getScriptTraceExtraTags(L));
	appendToLog(L);

	return 0;
}

static int base_warn(lua_State* L)
{
	Log.Warning("&&", getScriptTraceExtraTags(L));
	appendToLog(L);

	return 0;
}

static int base_appendlog(lua_State* L)
{
	Log.Append("&&", getScriptTraceExtraTags(L));
	appendToLog(L);

    return 0;
}

static const luaL_Reg base_funcs[] =
{
    { "print", base_print },
	{ "warn", base_warn },
    { "appendlog", base_appendlog },
    { NULL, NULL }
};

int luhxopen_base(lua_State* L)
{
    luaL_register(L, "_G", base_funcs);

    return 1;
}
