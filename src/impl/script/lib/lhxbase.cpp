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

	// TODO a kind of hack to get what script we're running as?
	lua_getglobal(L, "script");
	Reflection::GenericValue script = ScriptEngine::L::LuaValueToGeneric(L, -1);
	GameObject* scriptObject = GameObject::FromGenericValue(script);
	// modules currently do not have a `script` global
	uint32_t scriptId = scriptObject ? scriptObject->ObjectId : PHX_GAMEOBJECT_NULL_ID;

	lua_yield(L, 1);
	lua_pushthread(L);

	auto& b = ScriptEngine::s_YieldedCoroutines.emplace_back(
		L,
		// make sure the coroutine doesn't get de-alloc'd before we resume it
		lua_ref(L, -1),
		scriptId,
		ScriptEngine::YieldedCoroutine::ResumptionMode::ScheduledTime
	);

	double curTime = GetRunningTime();
	b.RmSchedule = { curTime, curTime + sleepTime };

	return -1;
}

static luaL_Reg base_funcs[] =
{
    { "print", base_print },
    { "appendlog", base_appendlog },
	{ "sleep", base_sleep },
    { NULL, NULL }
};

int luhxopen_base(lua_State* L)
{
    luaL_register(L, "_G", base_funcs);

    return 1;
}
