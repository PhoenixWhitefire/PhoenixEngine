#include "script/luhx.hpp"
#include "Engine.hpp"

static int engine_getwindowsize(lua_State* L)
{
    Engine* eng = Engine::Get();
    lua_pushinteger(L, eng->WindowSizeX);
    lua_pushinteger(L, eng->WindowSizeY);

    return 2;
}

static int engine_setwindowsize(lua_State* L)
{
    Engine::Get()->ResizeWindow(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2));
    return 0;
}

static int engine_isheadless(lua_State* L)
{
    lua_pushboolean(L, Engine::Get()->IsHeadlessMode);
    return 1;
}

static int engine_isfullscreen(lua_State* L)
{
    Engine* eng = Engine::Get();
    lua_pushboolean(L, eng->IsFullscreen);
    
    return 1;
}

static int engine_setfullscreen(lua_State* L)
{
    Engine::Get()->SetIsFullscreen(luaL_optboolean(L, 1, true));
    return 0;
}

static int engine_getvsync(lua_State* L)
{
    Engine* eng = Engine::Get();
    lua_pushboolean(L, eng->VSync);

    return 1;
}

static int engine_setvsync(lua_State* L)
{
    Engine::Get()->VSync = luaL_checkboolean(L, 1);
    return 0;
}

static int engine_framerate(lua_State* L)
{
    lua_pushinteger(L, Engine::Get()->FramesPerSecond);
    return 1;
}

static int engine_getmaxframerate(lua_State* L)
{
    Engine* eng = Engine::Get();
    lua_pushinteger(L, eng->FpsCap);

    return 1;
}

static int engine_setmaxframerate(lua_State* L)
{
    Engine::Get()->FpsCap = luaL_checkinteger(L, 1);
    return 1;
}

static int engine_exit(lua_State* L)
{
    Engine* eng = Engine::Get();
    eng->ExitCode = luaL_optinteger(L, 1, 0);
    eng->Close();

    return 0;
}

static int engine_dwireframes(lua_State* L)
{
    Engine* eng = Engine::Get();
    lua_pushboolean(L, eng->DebugWireframeRendering);

    if (lua_isboolean(L, 1))
        eng->DebugWireframeRendering = lua_toboolean(L, 1);

    return 1;
}

static int engine_daabbs(lua_State* L)
{
    Engine* eng = Engine::Get();
    lua_pushboolean(L, eng->DebugAabbs);

    if (lua_isboolean(L, 1))
        eng->DebugAabbs = lua_toboolean(L, 1);

    return 1;
}

static luaL_Reg engine_funcs[] =
{
    { "getwindowsize", engine_getwindowsize },
    { "setwindowsize", engine_setwindowsize },
    { "isheadless", engine_isheadless },
    { "getfullscreen", engine_isfullscreen },
    { "setfullscreen", engine_setfullscreen },
    { "getvsync", engine_getvsync },
    { "setvsync", engine_setvsync },
    { "framerate", engine_framerate },
    { "getmaxframerate", engine_getmaxframerate },
    { "setmaxframerate", engine_setmaxframerate },
    { "exit", engine_exit },
    { "dwireframes", engine_dwireframes },
    { "daabbs", engine_daabbs },
    { NULL, NULL }
};

int luhxopen_engine(lua_State* L)
{
    luaL_register(L, LUHX_ENGINELIBNAME, engine_funcs);

    return 1;
}
