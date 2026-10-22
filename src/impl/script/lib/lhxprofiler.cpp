#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "script/TracyLuau.hpp"

static ScriptEngine::StateUserdata* getVmUserdata(lua_State* L)
{
    return (ScriptEngine::StateUserdata*)lua_getthreaddata(lua_mainthread(L));
}

static int profiler_zonebegin(lua_State* L)
{
    std::string zoneName;
    tracy::LuauZoneBeginNImpl(L, &zoneName);

    ScriptEngine::StateUserdata* vmud = getVmUserdata(L);
    vmud->UnfinishedProfilerZones.push(zoneName);

    return 0;
}

static int profiler_zoneend(lua_State* L)
{
    tracy::LuauZoneEnd(L);

    ScriptEngine::StateUserdata* vmud = getVmUserdata(L);
    vmud->UnfinishedProfilerZones.pop();

    return 0;
}

static const luaL_Reg profiler_funcs[] = {
    { "zonebegin", profiler_zonebegin },
    { "zoneend", profiler_zoneend, },
    { "zonetext", tracy::LuauZoneText },
    { "message", tracy::LuauMessage },

    { NULL, NULL }
};

int luhxopen_profiler(lua_State* L)
{
    return luhx_makemodule(L, profiler_funcs);
}
