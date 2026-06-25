#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "script/TracyLuau.hpp"

// FROM: `ldblib.cpp` 15/08/2025
static lua_State* getthread(lua_State* L, int* arg)
{
    if (lua_isthread(L, 1))
    {
        *arg = 1;
        return lua_tothread(L, 1);
    }
    else
    {
        *arg = 0;
        return L;
    }
}

static int debug_traceback(lua_State* L)
{
    int arg;
    lua_State* L1 = getthread(L, &arg);
    const char* msg = luaL_optstring(L, arg + 1, NULL);
    int level = luaL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    luaL_argcheck(L, level >= 0, arg + 2, "level can't be negative");

	std::string trace;
	ScriptEngine::L::DumpStacktrace(L, &trace, level, msg);

	lua_pushlstring(L, trace.data(), trace.size());
	return 1;
}

// FROM: `luau/tests/Conformance.test.cpp` line 1543 for `breakpoint` function
static int debug_breakpoint(lua_State* L)
{
	if (lua_gettop(L) == 0)
		return lua_break(L);

	int line = luaL_checkinteger(L, 1);
    bool enabled = luaL_optboolean(L, 2, true);

    lua_Debug ar = {};
    lua_getinfo(L, lua_stackdepth(L) - 1, "f", &ar);

    lua_pushinteger(L, lua_breakpoint(L, -1, line, enabled));
    return 1;
}

static ScriptEngine::L::StateUserdata* getVmUserdata(lua_State* L)
{
    return (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(L));
}

static int debug_zonebegin(lua_State* L)
{
    std::string zoneName;
    int retCount = tracy::LuauZoneBeginNImpl(L, &zoneName);

    ScriptEngine::L::StateUserdata* vmud = getVmUserdata(L);
    vmud->UnfinishedProfilerZones.push(zoneName);

    return retCount;
}

static int debug_zoneend(lua_State* L)
{
    int retCount = tracy::LuauZoneEnd(L);

    ScriptEngine::L::StateUserdata* vmud = getVmUserdata(L);
    vmud->UnfinishedProfilerZones.pop();

    return retCount;
}

const luaL_Reg xdebug_funcs[] = {
    { "traceback", debug_traceback },
    { "breakpoint", debug_breakpoint },

    { "zonebegin", debug_zonebegin },
    { "zoneend", debug_zoneend, },
    { "zonetext", tracy::LuauZoneText },
    { "profilemessage", tracy::LuauMessage },

    { NULL, NULL }
};

int luhxopen_debug(lua_State* L)
{
    luaL_register(L, LUA_DBLIBNAME, xdebug_funcs);

    return 1;
}
