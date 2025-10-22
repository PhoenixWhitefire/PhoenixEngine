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
	std::string readError;
	std::string contents = FileRW::ReadFile(path, &readSuccess, &readError);

	if (!readSuccess)
	{
		lua_pushnil(L);
		lua_pushlstring(L, readError.data(), readError.size());
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

static int base_defer(lua_State* L)
{
	luaL_argexpected(L, lua_type(L, 1) == LUA_TFUNCTION || lua_type(L, 1) == LUA_TTHREAD, 1, "function or thread (coroutine)");

	int numArgs = lua_gettop(L);
	int numFnArgs = numArgs > 2 ? numArgs - 2 : 0;

	lua_State* DL = nullptr;
	int ref = 0;

	double sleepTime = luaL_optnumber(L, 2, 0.f);

	if (lua_type(L, 1) == LUA_TFUNCTION)
	{
		DL = lua_newthread(L);
		ref = lua_ref(L, -1);
		lua_pop(L, 1);

		if (numFnArgs > 0)
		{
			lua_remove(L, 2);
			lua_xmove(L, DL, numFnArgs + 1); // L:           | DL: fn, args
		}
		else
		{
			lua_pushvalue(L, 1);
			lua_xmove(L, DL, 1);
		}
	}
	else
	{
		DL = lua_tothread(L, 1);
		ref = lua_ref(L, 1);

		lua_xmove(L, DL, numFnArgs);
	}

	ScriptEngine::YieldedCoroutine yc =
	{
		.Coroutine = DL,
		.CoroutineReference = ref,
		.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::ScheduledTime,
		.RmSchedule = {
			.YieldedAt = GetRunningTime(),
			.ResumeAt = GetRunningTime() + sleepTime,
			.NumRetVals = numFnArgs,
			.PushSleptTime = false
		}
	};
	ScriptEngine::s_YieldedCoroutines.push_back(yc);

	return 0;
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
	{ "defer", base_defer },
    { NULL, NULL }
};

int luhxopen_base(lua_State* L)
{
    luaL_register(L, "_G", base_funcs);

    return 1;
}
