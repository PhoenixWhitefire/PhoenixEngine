#include <imgui.h>

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
    lua_pushboolean(L, UserInput::IsKeyDown(luaL_checkinteger(L, 1)));
	return 1;
}

static int input_mousedown(lua_State* L)
{
	lua_pushboolean(L, UserInput::IsMouseButtonDown(luaL_checkinteger(L, 1)));
	return 1;
}

static int input_mouseposition(lua_State* L)
{
    double mx = 0;
    double my = 0;

    glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);

    lua_pushvector(L, (float)mx, (float)my, 0.f);
    return 1;
}

static int input_setmouseposition(lua_State* L)
{
    const float* v = luaL_checkvector(L, 1);
    glfwSetCursorPos(glfwGetCurrentContext(), v[0], v[1]);
    return 0;
}

static int input_mousegrabbed(lua_State* L)
{
    lua_pushboolean(L, glfwGetInputMode(glfwGetCurrentContext(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED);
    return 1;
}

static int input_setmousegrabbed(lua_State* L)
{
    glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, luaL_checkboolean(L, 1) ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    return 0;
}

static int input_cursorvisible(lua_State* L)
{
    lua_pushboolean(L, glfwGetInputMode(glfwGetCurrentContext(), GLFW_CURSOR) == GLFW_CURSOR_NORMAL);
    return 1;
}

static int input_setcursorvisible(lua_State* L)
{
    if (luaL_checkboolean(L, 1))
    {
        glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    }
    else
    {
        glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }
    
    return 0;
}

static luaL_Reg input_funcs[] =
{
    { "keypressed", input_keypressed },
    { "mousedown", input_mousedown },
    { "mouseposition", input_mouseposition },
    { "setmouseposition", input_setmouseposition },
    { "mousegrabbed", input_mousegrabbed },
    { "setmousegrabbed", input_setmousegrabbed },
    { "guihandledk", input_guihandledk },
    { "guihandledm", input_guihandledm },
    { "guihandled", input_guihandled },
    { "cursorvisible", input_cursorvisible },
    { "setcursorvisible", input_setcursorvisible },
    { NULL, NULL }
};

int luhxopen_input(lua_State* L)
{
    if (Engine::Get()->IsHeadlessMode)
    {
        luaL_Reg* l = &input_funcs[0];
        for (; l->name; l++)
        {
            l->func = [](lua_State* L)
                -> int
                {
                    luaL_error(L, "Function cannot be called in headless mode");
                };
        }
    }

    luaL_register(L, LUHX_INPUTLIBNAME, input_funcs);

    return 1;
}
