#include <lualib.h>

#include "script/luhx.hpp"

static const luaL_Reg luhxlibs[] =
{
    { "", luhxopen_base },
    { LUHX_FSLIBNAME, luhxopen_fs },
    { LUHX_CONFLIBNAME, luhxopen_conf },
    { LUHX_IMGUILIBNAME, luhxopen_imgui },
    { LUHX_INPUTLIBNAME, luhxopen_input },
    { LUHX_JSONLIBNAME, luhxopen_json },
    { LUHX_MESHLIBNAME, luhxopen_mesh },
    { LUHX_MODELLIBNAME, luhxopen_model },
    { LUHX_SCENELIBNAME, luhxopen_scene },
    { LUHX_ENGINELIBNAME, luhxopen_engine },

    { LUHX_COLORLIBNAME, luhxopen_Color },
    { LUHX_MATRIXLIBNAME, luhxopen_Matrix },
    { LUHX_GAMEOBJECTLIBNAME, luhxopen_GameObject },

    { "", luhxopen_EventSignal },
    { "", luhxopen_EventConnection },

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
