#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "Log.hpp"

static int base_print(lua_State* L)
{
    // FROM:
	// `luaB_print`
	// `Luau/VM/src/lbaselib.cpp`
	// 11/11/2024

	Log::Info("&&");

	int n = lua_gettop(L); // number of arguments
	for (int i = 1; i <= n; i++)
	{
		size_t l;
		const char* s = luaL_tolstring(L, i, &l); // convert to string using __tostring et al

		if (i > 1)
			Log::Append(" &&");
		else
			Log::Append("&&");

		if (i < n)
			Log::Append(std::string(s) + "&&");
		else
			Log::Append(s);

		lua_pop(L, 1); // pop result
	}

	return 0;
}

static int base_appendlog(lua_State* L)
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
    
		if (i > 1)
			Log::Append(" &&");
		else
			Log::Append("&&");
    
		if (i < n)
			Log::Append(std::string(s) + "&&");
		else
			Log::Append(s);
    
		lua_pop(L, 1); // pop result
	}

    return 0;
}

static int base_sleep(lua_State* L)
{
	double sleepTime = luaL_checknumber(L, 1);

	ScriptEngine::L::Yield(
		L,
		1,
		[sleepTime](ScriptEngine::YieldedCoroutine& yc)
		{
			double curTime = GetRunningTime();
			yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::ScheduledTime;
			yc.RmSchedule = { curTime, curTime + sleepTime };
		}
	);

	return -1;
}

// FROM: `luau/tests/Conformance.test.cpp` line 1364 for `breakpoint` function
static int base_breakpoint(lua_State* L)
{
	int line = luaL_checkinteger(L, 1);
    bool enabled = luaL_optboolean(L, 2, true);

    lua_Debug ar = {};
    lua_getinfo(L, lua_stackdepth(L) - 1, "f", &ar);

    lua_breakpoint(L, -1, line, enabled);
    return 0;
}

static luaL_Reg base_funcs[] =
{
    { "print", base_print },
    { "appendlog", base_appendlog },
	{ "sleep", base_sleep },
	{ "breakpoint", base_breakpoint },
    { NULL, NULL }
};

int luhxopen_base(lua_State* L)
{
    luaL_register(L, "_G", base_funcs);

    return 1;
}
