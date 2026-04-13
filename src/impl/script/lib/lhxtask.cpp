#include <luau/VM/src/lstate.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "ThreadManager.hpp"
#include "FileRW.hpp"

static int task_wait(lua_State* L)
{
	double sleepTime = luaL_optnumber(L, 1, 0.f);

	return ScriptEngine::L::Yield(
		L,
		1,
		[sleepTime](ScriptEngine::YieldedCoroutine& yc)
		{
			double curTime = GetRunningTime();
			yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Wait;
			yc.RmWait = { .YieldedAt = curTime, .ResumeAt = curTime + sleepTime };
		}
	);
}

static void schedule(lua_State* L, double sleepTime, int taskStackIndex, int numFnArgs)
{
    int taskType = lua_type(L, taskStackIndex);
    luaL_argexpected(L, taskType == LUA_TFUNCTION || taskType == LUA_TTHREAD, taskStackIndex, "function or thread (coroutine)");

    lua_State* DL = nullptr;
	int ref = 0;

	if (taskType == LUA_TFUNCTION)
	{
		DL = lua_newthread(L);
		lua_xpush(L, DL, taskStackIndex);
		ref = lua_ref(L, -1);
		lua_pop(L, 1);
	}
	else
	{
		DL = lua_tothread(L, taskStackIndex);
		ref = lua_ref(L, taskStackIndex);
	}

	lua_State* arguments = lua_newthread(DL);
	int argsRef = lua_ref(DL, -1);
	lua_pop(DL, 1);

	lua_xmove(L, arguments, numFnArgs);
	assert(lua_gettop(arguments) == numFnArgs);

	lua_getglobal(L, "game");
	Reflection::GenericValue dmgv = ScriptEngine::L::ToGeneric(L, -1);
	GameObject* dm = GameObjectManager::Get()->FromGenericValue(dmgv);

	ScriptEngine::YieldedCoroutine yc = {
		.Coroutine = DL,
		.CoroutineReference = ref,
		.DataModel = dm,
		.RmDeferred = {
			.ResumeAt = GetRunningTime() + sleepTime,
			.Arguments = arguments,
			.ArgumentsRef = argsRef
		},
		.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Deferred
	};
	ScriptEngine::s_YieldedCoroutines.push_back(yc);
}

static int task_defer(lua_State* L)
{
    int numFnArgs = lua_gettop(L) - 1;
    schedule(L, 0.f, 1, numFnArgs);

    return 0;
}

static int task_delay(lua_State* L)
{
    double sleepTime = luaL_checknumber(L, 1);
    int numFnArgs = lua_gettop(L) - 2;
    schedule(L, sleepTime, 2, numFnArgs);

    return 0;
}

static int task_load(lua_State* L)
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
	ScriptEngine::L::DumpStacktrace(L, &((ScriptEngine::L::StateUserdata*)ML->userdata)->SpawnTrace);

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

static int task_loadfile(lua_State* L)
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
		lua_pushvalue(L, lua_upvalueindex(1)); // `task.load`
		lua_pushlstring(L, contents.data(), contents.size());
		lua_pushstring(L, (std::string("@") + chname).c_str());

		lua_call(L, 2, 2);
	}

	return 2;
}

static int parallelPforkDummy(lua_State* L)
{
	lua_pushboolean(L, true);
	return 1;
}

static void runParallel(lua_State* PL, const std::vector<Reflection::GenericValue>& args, const std::string& script, const std::string& fname)
{
	bool readSuccess = false;
	std::string fileContents = FileRW::ReadFile(script, &readSuccess);

	if (!readSuccess)
	{
		Log.ErrorF("Failed to read '{}' to run it in parallel: {}", script, fileContents);
		lua_close(PL);
		return;
	}

	std::string chname = "@" + script;

	if (ScriptEngine::CompileAndLoad(PL, fileContents, chname) == 0)
	{
		lua_setreadonly(PL, LUA_GLOBALSINDEX, false);

		int status = lua_resume(PL, PL, 0);
		if (status != LUA_OK)
		{
			Log.ErrorF("Script {} did not finish running its main body in parallel (status: {}){}", chname, status, lua_isstring(PL, -1) ? std::format(": {}", lua_tostring(PL, -1)) : "");
		}
		else
		{
			if (lua_getglobal(PL, fname.c_str()) != LUA_TFUNCTION)
			{
				Log.ErrorF("{}: Parallel function '{}' was not found or was not a function in the global scope", chname, fname);
				lua_close(PL);
				return;
			}

			for (const Reflection::GenericValue& gv : args)
				ScriptEngine::L::PushGenericValue(PL, gv);

			status = lua_pcall(PL, args.size(), 0, 0);

			if (status != LUA_OK)
			{
				std::string errstr = std::format("{}: Parallel function '{}' did not finish running (status: {})", chname, fname, status);
				Log.ErrorF("{}{}", errstr, status == LUA_ERRRUN ? std::format(": {}", lua_tostring(PL, -1)) : "");
			}
		}
	}
	else
	{
		Log.ErrorF("{}: {}", chname, lua_tostring(PL, -1));
	}

	lua_close(PL);
}

static int task__pfork(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);

	lua_Debug ar = {};
	std::string script;
	std::string fname;

	if (!lua_getinfo(L, -lua_gettop(L), "n", &ar))
		luaL_error(L, "Unable to fetch function name");

	if (!ar.name)
		luaL_error(L, "No name associated with function, be sure to use the `function foo(...)` syntax instead of `foo = function(...)`");

	if (lua_getglobal(L, ar.name) != LUA_TFUNCTION)
		luaL_error(L, "'%s' was not a global function", ar.name);

	lua_pop(L, 1);
	fname = ar.name;

	if (!lua_getinfo(L, 1, "s", &ar))
		luaL_error(L, "Unable to fetch script source file");

	if (!ar.short_src)
		luaL_error(L, "Script source not available");

	script = ar.short_src;

	std::vector<Reflection::GenericValue> args;
	args.reserve(lua_gettop(L) - 1);

	for (int i = 2; i <= lua_gettop(L); i++)
		args.push_back(ScriptEngine::L::ToGeneric(L, i));

	lua_State* PL = ScriptEngine::L::Create(script + "!_pfork_VM");

	lua_getglobal(PL, "task");
	lua_setreadonly(PL, -1, false);
	lua_pushcfunction(
		PL,
		parallelPforkDummy,
		"task._pfork parallel"
	);
	lua_setfield(PL, -2, "_pfork");
	lua_setreadonly(PL, -1, true);
	lua_pop(PL, 1);

	// fails to compile :)
	//ThreadManager::Get()->Dispatch([args, chname, script, fname]()

	std::jthread(runParallel, PL, args, script, fname).detach();

	lua_pushboolean(L, false);
	return 1;
}

const luaL_Reg task_funcs[] = {
    { "wait", task_wait },
    { "defer", task_defer },
    { "delay", task_delay },
	{ "load", task_load },
	{ "_pfork", task__pfork },

    { NULL, NULL }
};

int luhxopen_task(lua_State* L)
{
    luaL_register(L, LUHX_TASKLIBNAME, task_funcs);

	lua_pushcfunction(L, task_load, "task.load internal");
	lua_pushcclosure(L, task_loadfile, "task.loadfile", 1);
	lua_setfield(L, -2, "loadfile");

    return 1;
}
