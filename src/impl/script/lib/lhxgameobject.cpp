#include <tracy/public/tracy/Tracy.hpp>
#include <luau/VM/src/lstate.h>

#include "script/ScriptEngine.hpp"
#include "script/luhx.hpp"

#define OBJECT_REG "OBJECT"

void luhx_pushgameobject(lua_State* L, const GameObject* Object)
{
    if (!Object)
		lua_pushnil(L); // null object properties are nil and falsey
	else
	{
		/*uint32_t* ptrToObj = (uint32_t*)lua_newuserdatadtor(
			L,
			sizeof(uint32_t),
			[](void* ptrId)
			{
				uint32_t id = *(uint32_t*)ptrId;

				if (GameObject* o = GameObject::GetObjectById(id))
					o->DecrementHardRefs(); // removes the reference in `::Create`
			}
		);*/

		lua_getfield(L, LUA_REGISTRYINDEX, OBJECT_REG);
		if (lua_isnil(L, -1))
		{
			lua_newtable(L);
			lua_pushvalue(L, -1);
			lua_setfield(L, LUA_REGISTRYINDEX, OBJECT_REG);
			lua_setreadonly(L, -1, false);
		}

		lua_pushinteger(L, *(const int32_t*)&Object->ObjectId);
		lua_gettable(L, -2); // OBJECT_REG[ObjectId]

		if (!lua_isnil(L, -1))
		{
			lua_remove(L, -2); // remove the registry sub-table
			return; // object already in the registry, return the same value (only way to make table key hashing work afaik)
		}
		lua_pop(L, 1); // dont need that nil

		uint32_t* ptrToObj = (uint32_t*)lua_newuserdata(L, sizeof(uint32_t));
		lua_pushinteger(L, *(const int32_t*)&Object->ObjectId);
		lua_pushvalue(L, -2);

		*ptrToObj = Object ? Object->ObjectId : PHX_GAMEOBJECT_NULL_ID;

		luaL_getmetatable(L, "GameObject");
		lua_setmetatable(L, -2);

		lua_settable(L, -4);
		lua_remove(L, -2); // remove the registry sub-table
	}
}

GameObject* luhx_checkgameobject(lua_State* L, int StackIndex)
{
    uint32_t* idptr = (uint32_t*)luaL_checkudata(L, StackIndex, "GameObject");
    return GameObject::GetObjectById(*idptr);
}

static int gameobject_new(lua_State* L)
{
    GameObject* newObject = GameObject::Create();
	if (lua_gettop(L) >= 1)
		newObject->Name = luaL_checkstring(L, 1);

	for (int i = 1; i <= lua_gettop(L); i++)
	{
		const char* n = luaL_checkstring(L, i);

		EntityComponent it = FindComponentTypeByName(n);
		if (it == EntityComponent::None)
			luaL_error(L, "Invalid component '%s'", n);

		newObject->AddComponent(it);
	}

    luhx_pushgameobject(L, newObject);
	return 1;
}

static int gameobject_fromId(lua_State* L)
{
	int oid = luaL_checkinteger(L, 1);
	luhx_pushgameobject(L, GameObject::GetObjectById((uint32_t)oid));

	return 1;
}

static const luaL_Reg gameobject_funcs[] =
{
    { "new", gameobject_new },
	{ "fromId", gameobject_fromId },
    { NULL, NULL }
};

static int obj_index(lua_State* L)
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	GameObject* obj = luhx_checkgameobject(L, 1);
	const char* key = luaL_checkstring(L, 2);

	ZoneText(key, strlen(key));

	if (strcmp(key, "Exists") == 0)
	{
		// whether or not it exists
		lua_pushboolean(L, obj ? 1 : 0);
		return 1;
	}

    if (!obj)
        luaL_error(L, "Tried to index '%s' of a deleted Game Object (use '.Exists' to check before accessing a member)", key);

    ReflectorRef ref;

	if (const Reflection::PropertyDescriptor* prop = obj->FindProperty(key, &ref))
	{
		Reflection::GenericValue gv = prop->Get(ref.Referred());
		assert(Reflection::TypeFits(prop->Type, gv.Type));

		ScriptEngine::L::PushGenericValue(L, gv);
	}

	else if (const Reflection::EventDescriptor* event = obj->FindEvent(key, &ref))
		luhx_pushsignal(L, event, ref, key);

	else
	{
		GameObject* child = obj->FindChild(key);

		if (child)
			luhx_pushgameobject(L, child);
		else
			// 18/05/2025
			// this is going to be an error because i spent an entire 26 seconds
			// trying to figure out why something wasnt working
			luaL_error(L, "No child or member '%s' of %s", key, obj->GetFullName().c_str());
	}

	return 1;
};

static int obj_newindex(lua_State* L)
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	GameObject* obj = luhx_checkgameobject(L, 1);
	const char* key = luaL_checkstring(L, 2);

	ZoneText(key, strlen(key));

	if (strcmp(key, "Exists") == 0)
		luaL_error(L, "%s", "'Exists' is read-only! - 21/12/2024");

    if (!obj)
	    luaL_error(L, "Cannot assign to property '%s' of a delete GameObject", key);

	if (const Reflection::PropertyDescriptor* prop = obj->FindProperty(key))
	{
		if (!prop->Set)
		{
			const char* argTypeName = luaL_typename(L, 3);
			const char* argAsString = luaL_tolstring(L, 3, nullptr);

			luaL_error(L,
				"Cannot set '%s' to '%s' (%s) because it is read-only",
				key, argAsString, argTypeName
			);
		}

		ScriptEngine::L::CheckType(L, prop->Type, 3);
		Reflection::GenericValue newValue = ScriptEngine::L::ToGeneric(L, 3);

		try
		{
			obj->SetPropertyValue(key, newValue);
		}
		catch (const std::runtime_error& err)
		{
			luaL_error(L, "Error while setting property '%s' of %s: %s", key, obj->GetFullName().c_str(), err.what());
		}
	}
	else
	{
		std::string fullname = obj->GetFullName();

		if (obj->FindChild(key))
			luaL_error(L,
				"Attempt to set invalid Member '%s' of '%s', although it has a child object with that name",
				key, fullname.c_str()
			);
		else
			luaL_error(L,
				"Attempt to set invalid Member '%s' of %s",
				key, fullname.c_str()
			);
	}

	return 0;
};

static int obj_namecall(lua_State* L)
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	GameObject* g = luhx_checkgameobject(L, 1);
	const char* k = L->namecall->data; // this is weird 10/01/2025

	if (!g)
		luaL_error(L, "Tried to call '%s' of a de-allocated GameObject with ID %u", k, *(uint32_t*)lua_touserdata(L, 1));

	ZoneText(k, strlen(k));

	ReflectorRef reflector;
	const Reflection::MethodDescriptor* func = g->FindMethod(k, &reflector);

	if (!func)
		luaL_error(L, "'%s' is not a valid method of %s", k, g->GetFullName().c_str());

	int numresults = 0;

	try
	{
		numresults = ScriptEngine::L::HandleMethodCall(
			L,
			func,
			reflector
		);
	}
	catch (const std::runtime_error& err)
	{
		luaL_error(L, "Error while invoking method '%s' of %s: %s", k, g->GetFullName().c_str(), err.what());
	}

	return numresults;
}

static int obj_tostring(lua_State* L)
{
	GameObject* object = luhx_checkgameobject(L, 1);

	if (object)
		lua_pushstring(L, object->GetFullName().c_str());
	else
		lua_pushstring(L, "<!Deleted GameObject!>");

	return 1;
};

static void createmetatable(lua_State* L)
{
    luaL_newmetatable(L, LUHX_GAMEOBJECTLIBNAME);

    lua_pushstring(L, "GameObject");
    lua_setfield(L, -2, "__type");

    lua_pushcfunction(L, obj_index, "GameObject.__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, obj_newindex, "GameObject.__newindex");
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, obj_namecall, "__namecall"); // leaving as "__namecall" SPECIFICALLY adds the method name to errors (check `currfuncname` in laux.cpp)
    lua_setfield(L, -2, "__namecall");

    lua_pushcfunction(L, obj_tostring, "GameObject.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pop(L, 1);
}

int luhxopen_GameObject(lua_State* L)
{
    luaL_register(L, LUHX_GAMEOBJECTLIBNAME, gameobject_funcs);
    createmetatable(L);

    lua_createtable(L, (int)EntityComponent::__count, 0);

	for (uint8_t i = 1; i < (int)EntityComponent::__count; i++)
	{
		lua_pushinteger(L, i);
		lua_pushlstring(L, s_EntityComponentNames[i].data(), s_EntityComponentNames[i].length());
		lua_settable(L, -3);
	}

	lua_setfield(L, -2, "validComponents");

    return 1;
}
