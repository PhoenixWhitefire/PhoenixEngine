#include <nljson.hpp>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"

static int json_parse(lua_State* L)
{
	nlohmann::json json = nlohmann::json::parse(luaL_checkstring(L, 1));
	ScriptEngine::L::PushJson(L, json);

    return 1;
}

static int json_encode(lua_State* L)
{
	int indent = luaL_optinteger(L, 2, 2);

	nlohmann::json json = ScriptEngine::L::ToJson(L, 1);
	std::string dumped = json.dump(indent);
	lua_pushlstring(L, dumped.data(), dumped.size());
	
	return 1;
}

static const luaL_Reg json_funcs[] =
{
	{ "parse", json_parse },
	{ "encode", json_encode },
	{ NULL, NULL }
};

int luhxopen_json(lua_State* L)
{
	luaL_register(L, LUHX_JSONLIBNAME, json_funcs);

	return 1;
}
