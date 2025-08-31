#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "asset/SceneFormat.hpp"
#include "FileRW.hpp"

static int scene_save(lua_State* L)
{
    Reflection::GenericValue rootNodesGv = ScriptEngine::L::LuaValueToGeneric(L, 1);
	const char* path = luaL_checkstring(L, 2);

	std::span<Reflection::GenericValue> rootNodesArray = rootNodesGv.AsArray();
	std::vector<GameObject*> rootNodes;

	for (const Reflection::GenericValue& gv : rootNodesArray)
		rootNodes.push_back(GameObject::FromGenericValue(gv));

	std::string fileContents = SceneFormat::Serialize(rootNodes, path);
    bool success = FileRW::WriteFileCreateDirectories(path, fileContents);

	lua_pushboolean(L, success);
    return 2;
}

static int scene_load(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);

	std::string fileContents = FileRW::ReadFile(path);

	bool deserializeSuccess = true;
	std::vector<GameObjectRef> rootNodes = SceneFormat::Deserialize(fileContents, &deserializeSuccess);

	if (!deserializeSuccess)
    {
        lua_pushnil(L);
		lua_pushstring(L, SceneFormat::GetLastErrorString().c_str());

        return 2;
    }

	std::vector<Reflection::GenericValue> convertedArray;

	for (GameObject* node : rootNodes)
		convertedArray.push_back(node->ToGenericValue());

	Reflection::GenericValue gv = convertedArray;
	ScriptEngine::L::PushGenericValue(L, gv);

	return 1;
}

static luaL_Reg scene_funcs[] =
{
    { "save", scene_save },
    { "load", scene_load },
    { NULL, NULL }
};

int luhxopen_scene(lua_State* L)
{
    luaL_register(L, LUHX_SCENELIBNAME, scene_funcs);

    return 1;
}
