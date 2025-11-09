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

static const std::unordered_map<char, int> s_KNameToCode =
{
    { 'w', GLFW_KEY_W },
    { 'a', GLFW_KEY_A },
    { 's', GLFW_KEY_S },
    { 'd', GLFW_KEY_D },
    { 'e', GLFW_KEY_E },
    { 'f', GLFW_KEY_F },
    { 'q', GLFW_KEY_Q },
    { 'r', GLFW_KEY_R },
    { 'm', GLFW_KEY_M },
    { 'i', GLFW_KEY_I },
    { 't', GLFW_KEY_T },
    { 'p', GLFW_KEY_P },
    { '1', GLFW_KEY_1 },
    { '2', GLFW_KEY_2 },
    { '3', GLFW_KEY_3 },
    { ' ', GLFW_KEY_SPACE }
};

static int input_keypressed(lua_State* L)
{
    if (lua_isnumber(L, 1))
    {
        lua_pushboolean(L, UserInput::IsKeyDown((int)luaL_checkinteger(L, 1)));
    }
    else if (lua_isstring(L, 1))
    {
        size_t len = 0;
        const char* kname = luaL_checklstring(L, 1, &len);

        if (len != 1)
            luaL_error(L, "Expected string of length 1, got %zi characters instead (String: '%s')", len, kname);

        const auto& it = s_KNameToCode.find(kname[0]);
        if (it == s_KNameToCode.end())
            luaL_error(L, "Unsupported key '%c'", kname[0]);

        lua_pushboolean(L, UserInput::IsKeyDown(it->second));
    }
    else
    {
        luaL_typeerror(L, 1, "string or number");
    }
	return 1;
}

static int input_mousedown(lua_State* L)
{
    size_t bstrlen = 0;
    const char* bstr = luaL_optlstring(L, 1, "b", &bstrlen);
    luaL_argcheck(L, bstrlen == 1, 1, "must be exactly 1 character long");
    char b = bstr[0];
    luaL_argcheck(L, b == 'e' || b == 'l' || b == 'r', 1, "must be either 'e', 'l', or 'r'");

    bool value = false;

    switch (b)
    {
    case 'e':
    {
        value = UserInput::IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT) || UserInput::IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT);
        break;
    }
    case 'l':
    {
        value = UserInput::IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
        break;
    }
    case 'r':
    {
        value = UserInput::IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT);
        break;
    }
    [[unlikely]] default: { assert(false); }
    }

	lua_pushboolean(L, value);

	return 1;
}

static int input_mouseposition(lua_State* L)
{
    double mx = 0;
    double my = 0;

    glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
    
    lua_pushnumber(L, mx);
    lua_pushnumber(L, my);
    return 2;
}

static int input_setmouseposition(lua_State* L)
{
    glfwSetCursorPos(glfwGetCurrentContext(), luaL_checknumber(L, 1), luaL_checknumber(L, 2));
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
