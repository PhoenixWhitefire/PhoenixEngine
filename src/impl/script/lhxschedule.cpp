// lhxschedule.cpp, 22/10/2026
#include <lualib.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"

static int schedule_frame(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);

    ScriptEngine::StateUserdata* vmud = (ScriptEngine::StateUserdata*)lua_getthreaddata(lua_mainthread(L));
    vmud->VM->PerFrame.push_back(lua_ref(L, 1));

    return 0;
}

static const luaL_Reg schedule_funcs[] = {
    { "frame", schedule_frame },
    { nullptr, nullptr }
};

int luhxopen_schedule(lua_State* L)
{
    return luhx_makemodule(L, schedule_funcs);
}
