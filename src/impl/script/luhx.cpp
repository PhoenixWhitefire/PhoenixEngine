#include <lualib.h>

#include "script/luhx.hpp"

static const luaL_Reg luhxlibs[] = {
    { "", luhxopen_base },
    { LUA_DBLIBNAME, luhxopen_debug},

    { LUHX_ENUMLIBNAME, luhxopen_Enum },
    { LUHX_COLORLIBNAME, luhxopen_Color },
    { LUHX_MATRIXLIBNAME, luhxopen_Matrix },
    { LUHX_GAMEOBJECTLIBNAME, luhxopen_GameObject },

    { "", luhxopen_EventSignal },
    { "", luhxopen_EventConnection },
    { "", luhxopen_InputEvent },

    { NULL, NULL }
};

void luhx_openlibs(lua_State* L)
{
    const luaL_Reg* lib = luhxlibs;
    for (; lib->func; lib++)
    {
        lua_pushcfunction(L, lib->func, NULL);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
}

void luhx_pushvector3(lua_State* L, const glm::vec3& v)
{
    lua_pushvector(L, v.x, v.y, v.z);
}

static int libsize(const luaL_Reg* l)
{
    int size = 0;
    for (; l->name; l++)
        size++;
    return size;
}

int luhx_makemodule(lua_State* L, const luaL_Reg *l)
{
    lua_createtable(L, 0, libsize(l));

    for (; l->name; l++)
    {
        lua_pushcfunction(L, l->func, l->name);
        lua_setfield(L, -2, l->name);
    }

    return 1;
}
