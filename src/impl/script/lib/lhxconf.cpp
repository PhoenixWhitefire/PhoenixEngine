#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "GlobalJsonConfig.hpp"
#include "FileRW.hpp"

static int conf_get(lua_State* L)
{
    const char* k = luaL_checkstring(L, 1);

	ScriptEngine::L::PushJson(L, EngineJsonConfig[k]);
	return 1;
}

static int conf_set(lua_State* L)
{
    const char* k = luaL_checkstring(L, 1);
	luaL_checkany(L, 2);

    ScriptEngine::L::PushJson(L, EngineJsonConfig[k]);

	EngineJsonConfig[k] = ScriptEngine::L::ToJson(L, 2);
	return 0;
}

static int conf_save(lua_State* L)
{
    bool success = FileRW::WriteFile("./phoenix.conf", EngineJsonConfig.dump(2));

    lua_pushboolean(L, success);
	return 1;
}

static luaL_Reg conf_funcs[] =
{
    { "get", conf_get },
    { "set", conf_set },
    { "save", conf_save },
    { NULL, NULL }
};

int luhxopen_conf(lua_State* L)
{
    luaL_register(L, LUHX_CONFLIBNAME, conf_funcs);

    return 1;
}
