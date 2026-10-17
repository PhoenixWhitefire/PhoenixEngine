// lhxecs.cpp, 17/10/2026
#include <lualib.h>

#include "script/luhx.hpp"
#include "script/LightUserdataTags.hpp"
#include "script/UserdataTags.hpp"
#include "datatype/Ecs.hpp"

static int ecs_pair(lua_State* L)
{
    Id predicate = luhx_checkid(L, 1);
    Id target = luhx_checkid(L, 2);

    luhx_pushid(L, pair(predicate, target));
    return 1;
}

int luhxopen_ecs(lua_State* L)
{
    lua_createtable(L, 0, 8);

    lua_pushcfunction(
        L,
        ecs_pair,
        "pair"
    );
    lua_setfield(L, -2, "pair");

    return 1;
}

World* luhx_checkworld(lua_State* L, int idx)
{
    return *(World**)luaL_checkudatatagged(L, idx, UserdataTag::World);
}

Id luhx_checkid(lua_State* L, int idx)
{
    if (lua_lightuserdatatag(L, idx) != LightUserdataTag::Id)
        luaL_typeerror(L, idx, "Id");

    return (Id)lua_tolightuserdatatagged(L, idx, LightUserdataTag::Id);
}

void luhx_pushid(lua_State* L, Id id)
{
    lua_pushlightuserdatatagged(L, (void*)id, LightUserdataTag::Id);
}
