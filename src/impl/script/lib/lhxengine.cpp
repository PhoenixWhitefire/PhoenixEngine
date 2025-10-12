#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "component/Script.hpp"
#include "Engine.hpp"

static int engine_windowsize(lua_State* L)
{
    Engine* eng = Engine::Get();

    lua_pushinteger(L, eng->WindowSizeX);
    lua_pushinteger(L, eng->WindowSizeY);
    return 2;
}

static int engine_setwindowsize(lua_State* L)
{
    Engine* eng = Engine::Get();

    eng->ResizeWindow(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2));
    return 0;
}

static int engine_isheadless(lua_State* L)
{
    Engine* eng = Engine::Get();

    lua_pushboolean(L, eng->IsHeadlessMode);
    return 1;
}

static int engine_fullscreen(lua_State* L)
{
    Engine* eng = Engine::Get();

    lua_pushboolean(L, eng->IsFullscreen);
    return 1;
}

static int engine_setfullscreen(lua_State* L)
{
    Engine* eng = Engine::Get();
    eng->SetIsFullscreen(luaL_optboolean(L, 1, true));

    return 0;
}

static int engine_vsync(lua_State* L)
{
    Engine* eng = Engine::Get();

    lua_pushboolean(L, eng->VSync);
    return 1;
}

static int engine_setvsync(lua_State* L)
{
    Engine* eng = Engine::Get();
    eng->VSync = luaL_checkboolean(L, 1);

    return 0;
}

static int engine_framerate(lua_State* L)
{
    Engine* eng = Engine::Get();

    lua_pushinteger(L, eng->FramesPerSecond);
    return 1;
}

static int engine_maxframerate(lua_State* L)
{
    Engine* eng = Engine::Get();

    lua_pushinteger(L, eng->FpsCap);
    return 1;
}

static int engine_setmaxframerate(lua_State* L)
{
    Engine* eng = Engine::Get();
    eng->FpsCap = luaL_checkinteger(L, 1);

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
    eng->DebugWireframeRendering = luaL_optboolean(L, 1, eng->DebugWireframeRendering);

    return 1;
}

static int engine_daabbs(lua_State* L)
{
    Engine* eng = Engine::Get();
    lua_pushboolean(L, eng->DebugAabbs);
    eng->DebugAabbs = luaL_optboolean(L, 1, eng->DebugAabbs);

    return 1;
}

static int engine_binddatamodel(lua_State* L)
{
    Engine* eng = Engine::Get();
    Reflection::GenericValue gv = ScriptEngine::L::ToGeneric(L, 1);
    eng->BindDataModel(GameObject::FromGenericValue(gv));

    return 0;
}

static int engine_physicstimescale(lua_State* L)
{
    Engine* eng = Engine::Get();
    int top = lua_gettop(L);
    lua_pushnumber(L, eng->PhysicsTimeScale);

    if (top == 1)
        eng->PhysicsTimeScale = luaL_checknumber(L, 1);

    else if (top > 1)
        luaL_error(L, "Too many arguments to `engine.physicstimescale`");

    return 1;
}

#include "InlayEditor.hpp"

static int engine_setexplorerroot(lua_State* L)
{
    InlayEditor::SetExplorerRoot(GameObject::FromGenericValue(ScriptEngine::L::ToGeneric(L, 1)));
    return 0;
}

static int engine_setexplorerselections(lua_State* L)
{
    std::vector<ObjectHandle> objects;

    lua_pushnil(L);
    while (lua_next(L, 1))
    {
        objects.push_back(GameObject::FromGenericValue(ScriptEngine::L::ToGeneric(L, -1)));

        lua_pop(L, 1);
    }

    InlayEditor::SetExplorerSelections(objects);

    return 0;
}

static int engine_pushlvm(lua_State* L)
{
    EcScript::PushLVM(luaL_checkstring(L, 1));

    return 0;
}

static int engine_poplvm(lua_State*)
{
    EcScript::PopLVM();

    return 0;
}

#include "GlobalJsonConfig.hpp"

static const std::string_view Tools[] =
{
    "Tool_Explorer",
    "Tool_Properties",
    "Tool_Materials",
    "Tool_Shaders",
    "Tool_Settings",
    "Tool_Info"
};

static int engine_toolnames(lua_State* L)
{
    lua_createtable(L, (int)std::size(Tools), 0);

    for (int i = 0; i < (int)std::size(Tools); i++)
    {
        lua_pushinteger(L, i);
        lua_pushlstring(L, Tools[i].data(), Tools[i].size());
        lua_settable(L, -3);
    }

    return 1;
}

static const char* checkValidTool(lua_State* L)
{
    const char* toolName = luaL_checkstring(L, 1);
    bool isValidTool = false;

    for (int i = 0; i < (int)std::size(Tools); i++)
    {
        if (strcmp(Tools[i].data(), toolName) == 0)
        {
            isValidTool = true;
            break;
        }
    }

    if (!isValidTool)
        luaL_error(L, "Invalid tool '%s'", toolName);

    return toolName;
}

static int engine_settoolenabled(lua_State* L)
{
    const char* requestedTool = checkValidTool(L);
    EngineJsonConfig[requestedTool] = (bool)luaL_checkboolean(L, 2);

    return 0;
}

static int engine_toolenabled(lua_State* L)
{
    const char* requestedTool = checkValidTool(L);
    const nlohmann::json& value = EngineJsonConfig[requestedTool];

    lua_pushboolean(L, value.is_null() ? true : (bool)value);
    return 1;
}

#include <SDL3/SDL_messagebox.h>

static int engine_showmessagebox(lua_State* L)
{
    size_t typestrlen = 0;
    const char* typestr = luaL_optlstring(L, 3, "", &typestrlen);

    if (typestrlen > 1)
        luaL_error(L, "Flags string should only be 1 character or less, but is %zi", typestrlen);

    SDL_MessageBoxFlags flags = 0;
    switch (*typestr)
    {
    case '\0':
    {
        break;
    }

    case 'i':
    {
        flags |= SDL_MESSAGEBOX_INFORMATION;
        break;
    }
    case 'w':
    {
        flags |= SDL_MESSAGEBOX_WARNING;
        break;
    }
    case 'e':
    {
        flags |= SDL_MESSAGEBOX_ERROR;
        break;
    }

    default:
    {
        luaL_error(L, "Invalid message box type '%c'", *typestr);
    }
    }

    PHX_ENSURE(SDL_ShowSimpleMessageBox(
        flags,
        luaL_checkstring(L, 1),
        luaL_checkstring(L, 2),
        SDL_GL_GetCurrentWindow()
    ));

    return 0;
}

static luaL_Reg engine_funcs[] =
{
    { "windowsize", engine_windowsize },
    { "setwindowsize", engine_setwindowsize },
    { "isheadless", engine_isheadless },
    { "fullscreen", engine_fullscreen },
    { "setfullscreen", engine_setfullscreen },
    { "getvsync", engine_vsync },
    { "setvsync", engine_setvsync },
    { "framerate", engine_framerate },
    { "maxframerate", engine_maxframerate },
    { "setmaxframerate", engine_setmaxframerate },
    { "exit", engine_exit },
    { "dwireframes", engine_dwireframes },
    { "daabbs", engine_daabbs },
    { "binddatamodel", engine_binddatamodel },
    { "physicstimescale", engine_physicstimescale },
    { "setexplorerroot", engine_setexplorerroot },
    { "setexplorerselections", engine_setexplorerselections },
    { "pushlvm", engine_pushlvm },
    { "poplvm", engine_poplvm },
    { "toolnames", engine_toolnames },
    { "settoolenabled", engine_settoolenabled },
    { "toolenabled", engine_toolenabled },
    { "showmessagebox", engine_showmessagebox },
    { NULL, NULL }
};

int luhxopen_engine(lua_State* L)
{
    luaL_register(L, LUHX_ENGINELIBNAME, engine_funcs);

    return 1;
}
