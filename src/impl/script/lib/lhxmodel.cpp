#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "asset/ModelImporter.hpp"

static int model_import(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	std::vector<ObjectRef> loaded = ModelLoader(path, PHX_GAMEOBJECT_NULL_ID).LoadedObjs;
	
	ScriptEngine::L::PushGenericValue(L, loaded.at(0)->ToGenericValue());

	/*
	lua_newtable(L);

	for (int index = 0; index < loadedRoots.size(); index++)
	{
		GameObject* object = loadedRoots[index];
		Reflection::GenericValue gv = object->ToGenericValue();

		lua_pushinteger(L, index);
		ScriptEngine::L::PushGenericValue(L, gv);

		lua_settable(L, -3);
	}
	*/

	return 1;
}

static luaL_Reg model_funcs[] =
{
	{ "import", model_import },
	{ NULL, NULL }
};

int luhxopen_model(lua_State* L)
{
	luaL_register(L, LUHX_MODELLIBNAME, model_funcs);

	return 1;
}
