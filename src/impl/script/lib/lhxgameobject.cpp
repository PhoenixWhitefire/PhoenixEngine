#include <tracy/public/tracy/Tracy.hpp>
#include <lualib.h>

#include "datatype/ComponentDependencies.hpp"
#include "script/ScriptEngine.hpp"
#include "script/luhx.hpp"
#include "script/UserdataTags.hpp"

#define OBJECT_REG "OBJECT"

void luhx_pushgameobject(lua_State* L, GameObject* Object)
{
    if (!Object)
    {
        lua_pushnil(L); // null objects are nil and false-y
        return;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, OBJECT_REG);
    if (lua_isnil(L, -1))
    {
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, OBJECT_REG);
    }

	lua_rawgeti(L, -1, *(const int32_t*)&Object->ObjectId); // OBJECT_REG[ObjectId]

    if (!lua_isnil(L, -1))
    {
        lua_remove(L, -2); // remove the registry sub-table
        return; // object already in the registry, return the same value (to make table key hashing/rawequal work)
    }
    lua_pop(L, 1); // dont need that nil

    Object->IncrementHardRefs();

    uint32_t* ptrToObj = (uint32_t*)lua_newuserdatataggedwithmetatable(
        L,
        sizeof(uint32_t),
		UserdataTag::GameObject
    );
	*ptrToObj = Object ? Object->ObjectId : PHX_GAMEOBJECT_NULL_ID;

    lua_pushvalue(L, -1);
	lua_rawseti(L, -3, *(const int32_t*)&Object->ObjectId);

    lua_remove(L, -2); // remove the registry sub-table

	// leave object at stack top
}

GameObject* luhx_checkgameobject(lua_State* L, int StackIndex)
{
    uint32_t* idptr = (uint32_t*)luaL_checkudatatagged(L, StackIndex, UserdataTag::GameObject);
    return GameObjectManager::Get()->FindById(*idptr);
}

static int gameobject_new(lua_State* L)
{
    ObjectHandle newObject = GameObjectManager::Get()->Create();

	if (lua_gettop(L) == 1)
	{
		luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE, 1, "expected table for argument 1, or 0 arguments");

		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			if (lua_type(L, -1) != LUA_TSTRING)
			{
				const char* vtn = luaL_typename(L, -1);
				luaL_error(L, "Non-string '%s' (%s) in Components table", luaL_tolstring(L, -1, nullptr), vtn);
			}

			const char* n = luaL_checkstring(L, -1);
			EntityComponent ec = FindComponentTypeByName(n);

			if (ec == EntityComponent::None)
				luaL_error(L, "Invalid component '%s'", n);
			newObject->AddComponent(ec);

			lua_pop(L, 1);
		}
	}

    luhx_pushgameobject(L, newObject.Dereference());
	return 1;
}

static int gameobject_fromTemplate(lua_State* L)
{
	size_t len = 0;
	const char* component = luaL_checklstring(L, 1, &len);

	EntityComponent ec = FindComponentTypeByName(std::string_view(component, len));
	if (ec == EntityComponent::None)
		luaL_error(L, "Invalid component name '%s'", component);

	ObjectHandle newObject = GameObjectManager::s_Create(ec);
	newObject->Name = std::string(component, len);

	for (EntityComponent ec : GetCommonDependenciesForComponent(ec))
		newObject->AddComponent(ec);

	luhx_pushgameobject(L, newObject.Dereference());
	return 1;
}

static int gameobject_fromId(lua_State* L)
{
	int oid = luaL_checkinteger(L, 1);

	luhx_pushgameobject(L, GameObjectManager::Get()->FindById((uint32_t)oid));
	return 1;
}

static const luaL_Reg gameobject_funcs[] = {
    { "new", gameobject_new },
	{ "fromId", gameobject_fromId },
	{ "fromTemplate", gameobject_fromTemplate },
    { NULL, NULL }
};

static int obj_index(lua_State* L)
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	GameObject* obj = luhx_checkgameobject(L, 1);
	const char* key = luaL_checkstring(L, 2);

	ZoneText(key, strlen(key));

    if (!obj)
        luaL_error(L, "Tried to index '%s' of a deleted Game Object (bug!)", key);

    ReflectorRef ref;

	if (const Reflection::PropertyDescriptor* prop = obj->FindProperty(key, &ref))
	{
        ScriptEngine::L::StateUserdata* ud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(L));
        if (ScriptEngine::ParallelVM* P = ud->PVM)
        {
            if (P->Desynchronized && !prop->ParallelReadSafe)
                luaL_error(L, "`%s` is not safe to read while desynchronized", key);
        }

		Reflection::GenericValue gv = prop->Get(ref.Referred());
		assert(Reflection::TypeFits(prop->Type, gv.Type));

		ScriptEngine::L::PushGenericValue(L, gv);
	}

	else if (const Reflection::EventDescriptor* event = obj->FindEvent(key, &ref))
		luhx_pushsignal(L, event, ref, key, UINT32_MAX);

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

	lua_Debug ar = {};
	lua_getinfo(L, 1, "sl", &ar);

	GameObject* obj = luhx_checkgameobject(L, 1);
	const char* key = luaL_checkstring(L, 2);

	ZoneText(key, strlen(key));

    if (!obj)
	    luaL_error(L, "Cannot assign to property '%s' of a deleted GameObject (bug!)", key);

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

        ScriptEngine::L::StateUserdata* ud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(L));
        if (ScriptEngine::ParallelVM* P = ud->PVM)
        {
            if (P->Desynchronized && !prop->ParallelWriteSafe)
                luaL_error(L, "`%s` is not safe to set while desynchronized", key);
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
	const char* k = lua_namecallatom(L, nullptr);

	if (!g)
		luaL_error(L, "Tried to call '%s' of a de-allocated GameObject with ID %u", k, *(uint32_t*)lua_touserdata(L, 1));

	ZoneText(k, strlen(k));

	ReflectorRef reflector;
	const Reflection::MethodDescriptor* func = g->FindMethod(k, &reflector);

	if (!func)
		luaL_error(L, "'%s' is not a valid method of %s", k, g->GetFullName().c_str());

    ScriptEngine::L::StateUserdata* ud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(L));
    if (ScriptEngine::ParallelVM* P = ud->PVM)
    {
        if (P->Desynchronized && !func->ParallelSafe)
            luaL_error(L, "`%s` is not safe to call while desynchronized", k);
    }

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
		luaL_error(L, "%s:%s: %s", g->GetFullName().c_str(), k, err.what());
	}

	// Note: May be -1 for yielding
	return numresults;
}

static int obj_tostring(lua_State* L)
{
	GameObject* object = luhx_checkgameobject(L, 1);

	if (object)
		lua_pushstring(L, object->GetFullName().c_str());
	else
		lua_pushliteral(L, "<!Deleted GameObject!>");

	return 1;
};

static void createmetatable(lua_State* L)
{
	lua_createtable(L, 0, 5);

    lua_pushliteral(L, "GameObject");
    lua_setfield(L, -2, "__type");

    lua_pushcfunction(L, obj_index, "GameObject.__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, obj_newindex, "GameObject.__newindex");
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, obj_namecall, "__namecall"); // leaving as "__namecall" SPECIFICALLY adds the method name to errors (check `currfuncname` in laux.cpp)
    lua_setfield(L, -2, "__namecall");

    lua_pushcfunction(L, obj_tostring, "GameObject.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_setuserdatametatable(L, UserdataTag::GameObject);
	lua_setuserdatadtor(L, UserdataTag::GameObject, [](lua_State*, void* ptrToId)
    {
		GameObjectManager* objectManager = GameObjectManager::Get();
		uint32_t targetId = *(uint32_t*)ptrToId;

		GameObject* target = objectManager->FindById(targetId);
		assert(target);

		target->DecrementHardRefs();
    });
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
