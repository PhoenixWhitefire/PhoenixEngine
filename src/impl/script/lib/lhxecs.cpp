// lhxecs.cpp, 17/10/2026
#include <lualib.h>

#include "script/luhx.hpp"
#include "script/LightUserdataTags.hpp"
#include "script/UserdataTags.hpp"
#include "datatype/Ecs.hpp"
#include "datatype/BuiltinComponents.hpp"

static int ecs_pair(lua_State* L)
{
    Id predicate = luhx_checkid(L, 1);
    Id target = luhx_checkid(L, 2);

    luhx_pushid(L, pair(predicate, target));
    return 1;
}

static int ecs_isPair(lua_State* L)
{
    Id maybePair = luhx_checkid(L, 1);

    lua_pushboolean(L, isPair(maybePair));
    return 1;
}

static int ecs_pairFirst(lua_State* L)
{
    Id pair = luhx_checkid(L, 1);
    luaL_argcheck(L, isPair(pair), 1, "expected a pair");

    luhx_pushid(L, pairFirst(pair));
    return 1;
}

static int ecs_pairSecond(lua_State* L)
{
    Id pair = luhx_checkid(L, 1);
    luaL_argcheck(L, isPair(pair), 1, "expected a pair");

    luhx_pushid(L, pairSecond(pair));
    return 1;
}

static const luaL_Reg ecs_funcs[] = {
    { "pair", ecs_pair },
    { "isPair", ecs_isPair },
    { "pairFirst", ecs_pairFirst },
    { "pairSecond", ecs_pairSecond },

    { nullptr, nullptr }
};

int luhxopen_ecs(lua_State* L)
{
    luhx_makemodule(L, ecs_funcs);

    for (Id c = Ec::start; c <= Ec::count; c++)
    {
        luhx_pushid(L, c);
        lua_setfield(L, -2, EcNames[c - 1].data());
    }

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

const ComponentUserdata* luhx_checkcomponent(lua_State* L, int idx)
{
    return (ComponentUserdata*)luaL_checkudatatagged(L, idx, UserdataTag::Component);
}

void luhx_pushcomponent(lua_State* L, ComponentRecordBase* cr, uint64_t index)
{
    ComponentUserdata* cu = (ComponentUserdata*)lua_newuserdatataggedwithmetatable(L, sizeof(ComponentUserdata), UserdataTag::Component);
    cu->cr = cr;
    cu->index = index;
}
