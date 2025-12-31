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
			Log::Append(" &&");
		else
			Log::Append("&&");

		if (i < n)
			Log::Append(std::string(s) + "&&", extraTag);
		else
			Log::Append(s, extraTag);

		lua_pop(L, 1); // pop result
	}
}

static int base_print(lua_State* L)
{
	// this is just so that the Output can attribute the log message to a script
	Log::Info("&&", getScriptTraceExtraTags(L));
	appendToLog(L);

	return 0;
}

static int base_warn(lua_State* L)
{
	Log::Warning("&&", getScriptTraceExtraTags(L));
	appendToLog(L);

	return 0;
}

static int base_appendlog(lua_State* L)
{
	Log::Append("&&", getScriptTraceExtraTags(L));
	appendToLog(L);

    return 0;
}

static int base_sleep(lua_State* L)
{
	double sleepTime = luaL_checknumber(L, 1);

	return ScriptEngine::L::Yield(
		L,
		1,
		[sleepTime](ScriptEngine::YieldedCoroutine& yc)
		{
			double curTime = GetRunningTime();
			yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::ScheduledTime;
			yc.RmSchedule = { curTime, curTime + sleepTime };
		}
	);
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
		lua_pushlstring(L, contents.data(), contents.size());
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
		.RmSchedule = {
			.YieldedAt = GetRunningTime(),
			.ResumeAt = GetRunningTime() + sleepTime,
			.NumRetVals = numFnArgs,
			.PushSleptTime = false
		},
		.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::ScheduledTime
	};
	ScriptEngine::s_YieldedCoroutines.push_back(yc);

	return 0;
}

#ifdef _WIN32

#define popen _popen
#define pclose _pclose

#endif

// 13/01/2025: https://stackoverflow.com/a/478960
// 02/12/2025: copied from Main.cpp
static std::string exec(const char* cmd)
{
	std::array<char, 128> buffer{ 0 };
	std::string result;
	FILE* pipe = popen(cmd, "r");

	if (!pipe)
		throw std::runtime_error("popen() failed!");

	while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
		result += buffer.data();
	
	pclose(pipe);

	return result;
}

static int base_shellexec(lua_State* L)
{
	const char* command = luaL_checkstring(L, 1);

	return ScriptEngine::L::Yield(
		L,
		1,
		[command](ScriptEngine::YieldedCoroutine& yc)
		{
			std::shared_future<std::string> f = std::async(std::launch::async, [command]()
			{
				std::string result = exec(command);
				return result;
			}).share();

			yc.RmPoll = [f](lua_State* L) -> int
			{
				if (!f.valid() || (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready))
					return -1;

				const std::string& ret = f.get();
				lua_pushlstring(L, ret.data(), ret.size());

				return 1;
			};
			yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Polled;
		}
	);
}

static const luaL_Reg base_funcs[] =
{
    { "print", base_print },
	{ "warn", base_warn },
    { "appendlog", base_appendlog },
	{ "sleep", base_sleep },
	{ "breakpoint", base_breakpoint },
	//{ "loadstring", base_loadstring },
	{ "loadthread", base_loadthread },
	{ "loadthreadfromfile", base_loadthreadfromfile },
	{ "defer", base_defer },
	{ "shellexec", base_shellexec },
    { NULL, NULL }
};

int luhxopen_base(lua_State* L)
{
    luaL_register(L, "_G", base_funcs);

    return 1;
}
