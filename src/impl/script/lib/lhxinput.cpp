#include <imgui.h>
#include <SDL3/SDL_mouse.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "UserInput.hpp"
#include "Utilities.hpp"
#include "Engine.hpp"

static int input_guihandledk(lua_State* L)
{
    lua_pushboolean(L, ImGui::GetIO().WantCaptureKeyboard);
    return 1;
}

static int input_guihandledm(lua_State* L)
{
    lua_pushboolean(L, ImGui::GetIO().WantCaptureMouse);
    return 1;
}

static int input_guihandled(lua_State* L)
{
    lua_pushboolean(L, ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantCaptureMouse);
    return 1;
}

static int input_keypressed(lua_State* L)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
        lua_pushboolean(L, false);
    else
    {
        const char* kname = luaL_checkstring(L, 1);
        lua_pushboolean(L, UserInput::IsKeyDown((SDL_Keycode)kname[0]));
    }
	return 1;
}

static int input_mousedown(lua_State* L)
{
    if (ImGui::GetIO().WantCaptureMouse)
    {
        lua_pushboolean(L, false);
        return 1;
    }

    size_t bstrlen = 0;
    const char* bstr = luaL_optlstring(L, 1, "b", &bstrlen);
    luaL_argcheck(L, bstrlen == 1, 1, "must be exactly 1 character long");
    char b = bstr[0];
    luaL_argcheck(L, b == 'b' || b == 'l' || b == 'r', 1, "must be either 'b', 'l', or 'r'");

    bool value = false;

    switch (b)
    {
    case 'b':
    {
        value = UserInput::IsMouseButtonDown(true) || UserInput::IsMouseButtonDown(false);
        break;
    }
    case 'l':
    {
        value = UserInput::IsMouseButtonDown(true);
        break;
    }
    case 'r':
    {
        value = UserInput::IsMouseButtonDown(false);
        break;
    }
    [[unlikely]] default: { assert(false); }
    }

	lua_pushboolean(L, value);

	return 1;
}

static int input_getmousepos(lua_State* L)
{
    float mx = 0;
	float my = 0;

	SDL_GetMouseState(&mx, &my);

	lua_pushnumber(L, mx);
	lua_pushnumber(L, my);

	return 2;
}

static int input_setmousepos(lua_State* L)
{
    SDL_WarpMouseInWindow(SDL_GL_GetCurrentWindow(), luaL_checknumber(L, 1), luaL_checknumber(L, 2));
    return 0;
}

static int input_ismousegrabbed(lua_State* L)
{
    lua_pushboolean(L, SDL_GetWindowMouseGrab(SDL_GL_GetCurrentWindow()));
    return 1;
}

static int input_setmousegrabbed(lua_State* L)
{
    SDL_SetWindowMouseGrab(SDL_GL_GetCurrentWindow(), luaL_checkboolean(L, 1));
    return 0;
}

static int input_iscursorvisible(lua_State* L)
{
    lua_pushboolean(L, SDL_CursorVisible());
    return 1;
}

static int input_setcursorvisible(lua_State* L)
{
    if (luaL_checkboolean(L, 1))
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
        SDL_ShowCursor();
    }
    else
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        SDL_HideCursor();
    }
    
    return 0;
}

static luaL_Reg input_funcs[] =
{
    { "keypressed", input_keypressed },
    { "mousedown", input_mousedown },
    { "getmousepos", input_getmousepos },
    { "setmousepos", input_setmousepos },
    { "ismousegrabbed", input_ismousegrabbed },
    { "setmousegrabbed", input_setmousegrabbed },
    { "guihandledk", input_guihandledk },
    { "guihandledm", input_guihandledm },
    { "guihandled", input_guihandled },
    { "iscursorvisible", input_iscursorvisible },
    { "setcursorvisible", input_setcursorvisible },
    { NULL, NULL }
};

int luhxopen_input(lua_State* L)
{
    luaL_register(L, LUHX_INPUTLIBNAME, input_funcs);

    return 1;
}
