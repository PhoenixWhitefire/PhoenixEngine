#include <tinyfiledialogs.h>

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
    luaL_checktype(L, 1, LUA_TTABLE);

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

static int engine_explorerselections(lua_State* L)
{
    const std::vector<ObjectHandle>& selections = InlayEditor::GetExplorerSelections();
    lua_createtable(L, selections.size(), 0);

    for (int i = 0; i < (int)selections.size(); i++)
    {
        lua_pushinteger(L, i + 1);
        ScriptEngine::L::PushGameObject(L, selections[i].Dereference());
        lua_settable(L, -3);
    }

    return 1;
}

static int engine_createvm(lua_State* L)
{
    ScriptEngine::RegisterNewVM(luaL_checkstring(L, 1));

    return 0;
}

static int engine_closevm(lua_State* L)
{
    const auto& it = ScriptEngine::VMs.find(luaL_checkstring(L, 1));
    if (it == ScriptEngine::VMs.end())
        luaL_error(L, "Invalid VM");

    ScriptEngine::L::Close(it->second.MainThread);
    ScriptEngine::VMs.erase(it);

    return 0;
}

static int engine_currentvm(lua_State* L)
{
    lua_pushlstring(L, ScriptEngine::CurrentVM.data(), ScriptEngine::CurrentVM.size());
    return 1;
}

static int engine_setcurrentvm(lua_State* L)
{
    std::string vm = luaL_checkstring(L, 1);
    if (ScriptEngine::VMs.find(vm) == ScriptEngine::VMs.end())
        luaL_error(L, "Invalid VM '%s'", vm.c_str());

    ScriptEngine::CurrentVM = vm;

    return 0;
}

static int engine_runinvm(lua_State* L)
{
    const char* vmName = luaL_checkstring(L, 1);

    const auto& it = ScriptEngine::VMs.find(vmName);
    if (it == ScriptEngine::VMs.end())
        luaL_error(L, "Invalid VM '%s'", vmName);

    const ScriptEngine::LuauVM& vm = it->second;

    const char* code = luaL_checkstring(L, 2);
	const char* chname = luaL_optstring(L, 3, code);

	lua_State* ML = lua_newthread(vm.MainThread);

    if (ScriptEngine::CompileAndLoad(ML, code, chname) == 0)
	{
		int result = lua_resume(ML, ML, 0);
        lua_pushinteger(L, result);

        const char* const ResultToMessage[] =
        {
            "ok",
            "yield",
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            "break"
        };

        const char* message = ResultToMessage[result];

        if (message)
            lua_pushstring(L, message);
        else
            lua_pushstring(L, lua_tostring(ML, -1));
	}
	else
	{
        lua_pushboolean(L, false);
        lua_pushstring(L, lua_tostring(ML, -1));
	}

    return 2;
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

static int engine_showmessagebox(lua_State* L)
{
    lua_pushinteger(L, tinyfd_messageBox(
        luaL_checkstring(L, 1),       // title
        luaL_checkstring(L, 2),       // message
        luaL_optstring(L, 3, "ok"),   // buttons
        luaL_optstring(L, 4, "info"), // icon
        luaL_optinteger(L, 5, 1)      // default button
    ));
    return 1;
}

#include "asset/TextureManager.hpp"

static int engine_unloadtexture(lua_State* L)
{
    TextureManager* texManager = TextureManager::Get();
    uint32_t resId = texManager->LoadTextureFromPath(luaL_checkstring(L, 1), false);
    texManager->UnloadTexture(resId);

    return 0;
}

static int engine_args(lua_State* L)
{
    Engine* engine = Engine::Get();

    lua_createtable(L, engine->argc, 0);
    for (int i = 0; i < engine->argc; i++)
    {
        lua_pushinteger(L, i + 1);
        lua_pushstring(L, engine->argv[i]);
        lua_settable(L, -3);
    }

    return 1;
}

static int engine_setviewport(lua_State* L)
{
    Engine* engine = Engine::Get();
    engine->ViewportPosition = { (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2) };
    engine->OverrideViewportSize = { (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4) };
    engine->OverrideDefaultViewport = true;

    return 0;
}

static int engine_resetviewport(lua_State* L)
{
    Engine* engine = Engine::Get();
    engine->OverrideDefaultViewport = false;
    engine->ViewportPosition = {};

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
    { "explorerselections", engine_explorerselections },
    { "createvm", engine_createvm },
    { "closevm", engine_closevm },
    { "currentvm", engine_currentvm },
    { "setcurrentvm", engine_setcurrentvm },
    { "runinvm", engine_runinvm },
    { "toolnames", engine_toolnames },
    { "settoolenabled", engine_settoolenabled },
    { "toolenabled", engine_toolenabled },
    { "showmessagebox", engine_showmessagebox },
    { "unloadtexture", engine_unloadtexture },
    { "args", engine_args },
    { "setviewport", engine_setviewport },
    { "resetviewport", engine_resetviewport },
    { NULL, NULL }
};

int luhxopen_engine(lua_State* L)
{
    luaL_register(L, LUHX_ENGINELIBNAME, engine_funcs);

    return 1;
}
