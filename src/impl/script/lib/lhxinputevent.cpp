// lhxinputevent.cpp, 02/01/2026
#include "script/luhx.hpp"
#include "Reflection.hpp"

#define IE_TYPENAME "InputEvent"

static int ie_index(lua_State* L)
{
    const Reflection::InputEvent* ie = static_cast<Reflection::InputEvent*>(luaL_checkudata(L, 1, IE_TYPENAME));
    const char* k = luaL_checkstring(L, 2);

    if (strcmp(k, "Type") == 0)
    {
        lua_pushinteger(L, (int)ie->Type);
        return 1;
    }

    switch (ie->Type)
    {
    case Reflection::InputEventType::NONE:
        luaL_error(L, "Bad input event");

    case Reflection::InputEventType::Key:
    {
        if (strcmp(k, "Key") == 0)
            lua_pushinteger(L, ie->Key.Button);
        else if (strcmp(k, "Scancode") == 0)
            lua_pushinteger(L, ie->Key.Scancode);
        else if (strcmp(k, "Action") == 0)
            lua_pushinteger(L, ie->Key.Action);
        else if (strcmp(k, "Modifiers") == 0)
            lua_pushinteger(L, ie->Key.Modifiers);
        else
            luaL_error(L, "Invalid property '%s' for Key input", k);
    }

    case Reflection::InputEventType::MouseButton:
    {
        if (strcmp(k, "Button") == 0)
            lua_pushinteger(L, ie->MouseButton.Button);
        else if (strcmp(k, "Action") == 0)
            lua_pushinteger(L, ie->MouseButton.Action);
        else if (strcmp(k, "Modifiers") == 0)
            lua_pushinteger(L, ie->MouseButton.Modifiers);
        else
            luaL_error(L, "Invalid property '%s' for Mouse Button input", k);
    }

    case Reflection::InputEventType::Scroll:
    {
        if (strcmp(k, "Delta") == 0)
            lua_pushvector(L, ie->Scroll.XOffset, ie->Scroll.YOffset, 0.f);
        else
            luaL_error(L, "Invalid property '%s' for Scroll input", k);
    }

    default:
        luaL_error(L, "Bad input event type '%i'", (int)ie->Type);
    }

    return 1;
}

void createmetatable(lua_State* L)
{
    luaL_newmetatable(L, IE_TYPENAME);

    lua_pushliteral(L, IE_TYPENAME);
    lua_setfield(L, -2, "__type");

    lua_pushcfunction(L, ie_index, "InputEvent.__index");
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1);
}

int luhxopen_inputevent(lua_State* L)
{
    createmetatable(L);
    return 0;
}
