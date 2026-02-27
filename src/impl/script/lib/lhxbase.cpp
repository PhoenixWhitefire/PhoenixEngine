#include <luau/VM/src/lstate.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

static uint32_t* toObjectUserData(lua_State* L, int i)
{
	if (void* p = lua_touserdata(L, i))
	{
		// FROM: `luau/VM/src/laux.cpp` line 128

		if (lua_getmetatable(L, i))
        {                                                     // does it have a metatable?
            lua_getfield(L, LUA_REGISTRYINDEX, "GameObject"); // get correct metatable
            if (lua_rawequal(L, -1, -2))
            {                  // does it have the correct mt?
                lua_pop(L, 2); // remove both metatables
                return (uint32_t*)p;
			}
		}
	}

	return nullptr;
}

static std::string getScriptTraceExtraTags(lua_State* L)
{
	lua_Debug ar = {};
	lua_getinfo(L, 1, "sln", &ar);

	return std::format("ScriptFunctionName:{},TextDocument:{},DocumentLine:{}", ar.name ? ar.name : "<anonymous>", ar.short_src , ar.currentline);
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
		std::string extraTag;
		if (uint32_t* gid = toObjectUserData(L, i))
			extraTag = std::format("Object:{}", *gid);

		size_t l;
		const char* s = luaL_tolstring(L, i, &l); // convert to string using __tostring et al

		if (i > 1)
			Log.Append(" &&");
		else
			Log.Append("&&");

		if (i < n)
			Log.Append(std::string(s) + "&&", extraTag);
		else
			Log.Append(s, extraTag);

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
