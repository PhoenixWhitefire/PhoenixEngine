#include <imgui.h>
#include <SDL3/SDL_mouse.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "UserInput.hpp"
#include "Engine.hpp"

static int input_keypressed(lua_State* L)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
    	lua_pushboolean(L, false);
    else
    {
    	const char* kname = luaL_checkstring(L, 1);
    	lua_pushboolean(L, UserInput::IsKeyDown(SDL_Keycode(kname[0])));
    }

	return 1;
}

static int input_mousedown(lua_State* L)
{
    if (ImGui::GetIO().WantCaptureMouse)
		lua_pushboolean(L, false);
	else
	{
        size_t bstrlen = 0;
        const char* bstr = luaL_optlstring(L, 1, "b", &bstrlen);
        luaL_argcheck(L, bstrlen == 1, 1, "must be exactly 1 character long");
        char b = bstr[0];
        luaL_argcheck(L, b == 'b' || b == 'l' || b == 'r', 1, "must be either 'b', 'l', or 'r'");

        bool value = false;

        if (b == 'b')
            value = UserInput::IsMouseButtonDown(true) || UserInput::IsMouseButtonDown(false);
        else if (b == 'l')
            value = UserInput::IsMouseButtonDown(true);
        else if (b == 'r')
            value = UserInput::IsMouseButtonDown(false);

		lua_pushboolean(L, value);
	}

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
    float mx = 0;
	float my = 0;

	SDL_GetMouseState(&mx, &my);

	lua_pushnumber(L, mx);
	lua_pushnumber(L, my);

	return 2;
}

static int input_setmouselocked(lua_State* L)
{
    lua_pushboolean(L, ScriptEngine::s_BackendScriptWantGrabMouse);
    ScriptEngine::s_BackendScriptWantGrabMouse = luaL_checkboolean(L, 1);

    return 1;
}

static luaL_Reg input_funcs[] =
{
    { "keypressed", input_keypressed },
    { "mousedown", input_mousedown },
    { "getmousepos", input_getmousepos },
    { "setmousepos", input_setmousepos },
    { "setmouselocked", input_setmouselocked },
    { NULL, NULL }
};

int luhxopen_input(lua_State* L)
{
    if (!Engine::Get()->IsHeadlessMode)
    {
        luaL_register(L, LUHX_INPUTLIBNAME, input_funcs);

        return 1;
    }
    else
        return 0;
}
