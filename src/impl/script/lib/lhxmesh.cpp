#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "asset/MeshProvider.hpp"

static int mesh_get(lua_State* L)
{
    const char* meshPath = luaL_checkstring(L, 1);

	MeshProvider* meshProvider = MeshProvider::Get();

	uint32_t meshId = meshProvider->LoadFromPath(meshPath, false, true);
	Mesh& mesh = meshProvider->GetMeshResource(meshId);

	lua_newtable(L);
	lua_newtable(L);

	for (size_t vi = 0; vi < mesh.Vertices.size(); vi++)
	{
		const Vertex& v = mesh.Vertices.at(vi);

		lua_pushinteger(L, static_cast<int32_t>(vi + 1));
		lua_newtable(L);

		lua_pushvector(L, v.Position.x, v.Position.y, v.Position.z);
		lua_setfield(L, -2, "Position");

		lua_pushvector(L, v.Normal.x, v.Normal.y, v.Normal.z);
		lua_setfield(L, -2, "Normal");

		lua_newtable(L);
		lua_pushnumber(L, v.Paint.x);
		lua_setfield(L, -2, "R");

		lua_pushnumber(L, v.Paint.y);
		lua_setfield(L, -2, "G");

		lua_pushnumber(L, v.Paint.z);
		lua_setfield(L, -2, "B");

		lua_pushnumber(L, v.Paint.w);
		lua_setfield(L, -2, "A");

		lua_setfield(L, -2, "Paint");
    
		lua_newtable(L);

		lua_pushinteger(L, 1);
		lua_pushnumber(L, v.TextureUV.x);
		lua_settable(L, -3);

		lua_pushinteger(L, 2);
		lua_pushnumber(L, v.TextureUV.y);
		lua_settable(L, -3);

		lua_setfield(L, -2, "UV");

		lua_settable(L, -3);
	}

	lua_setfield(L, -2, "Vertices");

	lua_newtable(L);

	for (uint32_t indexIndex = 0; indexIndex < mesh.Indices.size(); indexIndex++)
	{
		lua_pushinteger(L, indexIndex + 1);
		lua_pushinteger(L, static_cast<int32_t>(mesh.Indices.at(indexIndex)));
		lua_settable(L, -3);
	}

	lua_setfield(L, -2, "Indices");

	return 1;
}

static int mesh_set(lua_State* L)
{
    const char* meshName = luaL_checkstring(L, 1);

	Mesh mesh;
	mesh.MeshDataPreserved = true;

	lua_getfield(L, -1, "Vertices");

	lua_pushnil(L);
	while (lua_next(L, -2) != 0)
	{
		lua_getfield(L, -1, "Position");
		glm::vec3 position = ScriptEngine::L::ToGeneric(L).AsVector3();
    
		lua_getfield(L, -2, "Normal");
		glm::vec3 normal = ScriptEngine::L::ToGeneric(L).AsVector3();
    
		lua_getfield(L, -3, "Paint");
		Reflection::GenericValue paintVal = ScriptEngine::L::ToGeneric(L);
		std::span<Reflection::GenericValue> paintgv = paintVal.AsArray();
		glm::vec4 paint{ paintgv[0].AsDouble(), paintgv[1].AsDouble(), paintgv[2].AsDouble(), paintgv[3].AsDouble() };
    
		lua_getfield(L, -4, "UV");
    
		std::vector<double> uvlist;
    
		lua_pushnil(L);
		while (lua_next(L, -2) != 0)
		{
			uvlist.push_back(lua_tonumber(L, -1));
			lua_pop(L, 1);
		}
    
		glm::vec2 uv =
		{
			uvlist.at(0),
			uvlist.at(1)
		};
    
		mesh.Vertices.emplace_back(Vertex{ position, normal, paint, uv });
    
		lua_pop(L, 5);
	}

	lua_getfield(L, -2, "Indices");

	lua_pushnil(L);
	while (lua_next(L, -2) != 0)
	{
		mesh.Indices.push_back(lua_tointeger(L, -1));
		lua_pop(L, 1);
	}

	MeshProvider* meshProvider = MeshProvider::Get();
	meshProvider->Assign(mesh, meshName, true);

	return 0;
}

static int mesh_save(lua_State* L)
{
    const char* internalName = luaL_checkstring(L, 1);
	const char* savePath = luaL_checkstring(L, 2);

	MeshProvider* meshProv = MeshProvider::Get();
	meshProv->Save(meshProv->LoadFromPath(internalName), savePath);

	return 0;
}

static luaL_Reg mesh_funcs[] =
{
    { "get", mesh_get },
    { "set", mesh_set },
    { "save", mesh_save },
    { NULL, NULL }
};

int luhxopen_mesh(lua_State* L)
{
    luaL_register(L, LUHX_MESHLIBNAME, mesh_funcs);

    return 1;
}
