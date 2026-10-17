// lhxworld.cpp, 17/10/2026
#include "script/luhx.hpp"
#include "script/UserdataTags.hpp"
#include "datatype/Ecs.hpp"
#include "Engine.hpp"

int luhxopen_world(lua_State* L)
{
    // Maybe this shouldn't exist... if we allow
    // worlds to be re-bound then the behavior can be unexpected.

    Engine* engine = Engine::Get();
    World** p = (World**)lua_newuserdatataggedwithmetatable(L, sizeof(World*), UserdataTag::World);
    *p = &engine->world;

    return 1;
}
