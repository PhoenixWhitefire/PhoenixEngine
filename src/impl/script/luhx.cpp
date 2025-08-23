#include <lualib.h>

#include "script/luhx.hpp"

static const luaL_Reg luhxlibs[] =
{
    { "", luhxopen_base },
    { LUHX_FSLIBNAME, luhxopen_fs },
    { LUHX_NETLIBNAME, luhxopen_net },
    { LUHX_CONFLIBNAME, luhxopen_conf },
    { LUHX_IMGUILIBNAME, luhxopen_imgui },
    { LUHX_INPUTLIBNAME, luhxopen_input },
    { LUHX_JSONLIBNAME, luhxopen_json },
    { LUHX_MESHLIBNAME, luhxopen_mesh },
    { LUHX_MODELLIBNAME, luhxopen_model },
    { LUHX_SCENELIBNAME, luhxopen_scene },
    { LUHX_WORLDLIBNAME, luhxopen_world },
    { LUHX_ENGINELIBNAME, luhxopen_engine },
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
