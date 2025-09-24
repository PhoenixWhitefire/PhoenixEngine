#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "FileRW.hpp"
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

// `loadthread` preferred?
/*
static int base_loadstring(lua_State* L)
{
	const char* code = luaL_checkstring(L, 1);
	const char* chname = luaL_optstring(L, 2, code);
	
	if (ScriptEngine::CompileAndLoad(L, code, chname) == 0)
		return 1; // return callable function

	else
		luaL_error(L, "loadstring: %s", luaL_checkstring(L, -1));
}
*/

static int base_loadthread(lua_State* L)
{
	const char* code = luaL_checkstring(L, 1);
	const char* chname = luaL_optstring(L, 2, code);
	
	// module needs to run in a new thread, isolated from the rest
	// note: we create ML on main thread so that it doesn't inherit environment of L
	lua_State* GL = lua_mainthread(L);
	lua_State* ML = lua_newthread(GL);
	lua_xmove(GL, L, 1);

	// new thread needs to have the globals sandboxed
	luaL_sandboxthread(ML);
	
	if (ScriptEngine::CompileAndLoad(ML, code, chname) == 0)
	{
		lua_pushthread(ML);
		lua_xmove(ML, L, 1);
		lua_pushnil(L); // error message is nil
	}
	else
	{
		lua_pushnil(L); // thread is nil
		lua_xmove(ML, L, 1); // move error onto L
	}

	return 2;
}

static int base_loadthreadfromfile(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	const char* chname = luaL_optstring(L, 1, path);

	bool readSuccess = false;
	std::string contents = FileRW::ReadFile(path, &readSuccess);

	if (!readSuccess)
	{
		lua_pushnil(L);
		lua_pushstring(L, "File could not be read");
	}
	else
	{
		lua_getglobal(L, "loadthread");
		lua_pushlstring(L, contents.data(), contents.size());
		lua_pushstring(L, (std::string("@") + chname).c_str());

		lua_call(L, 2, 2);
	}

	return 2;
}

static luaL_Reg base_funcs[] =
{
    { "print", base_print },
    { "appendlog", base_appendlog },
	{ "sleep", base_sleep },
	{ "breakpoint", base_breakpoint },
	//{ "loadstring", base_loadstring },
	{ "loadthread", base_loadthread },
	{ "loadthreadfromfile", base_loadthreadfromfile },
    { NULL, NULL }
};

int luhxopen_base(lua_State* L)
{
    luaL_register(L, "_G", base_funcs);

    return 1;
}
