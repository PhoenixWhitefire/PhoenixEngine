#include <luau/VM/src/lstate.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"

static int conn_namecall(lua_State* L)
{
	if (strcmp(L->namecall->data, "Disconnect") == 0)
	{
		EventConnectionData* ec = (EventConnectionData*)luaL_checkudata(L, 1, "EventConnection");

		if (ec->ConnectionId == UINT32_MAX)
			luaL_error(L, "Event Connection was already disconnected!");

		ec->Event->Disconnect(ec->Reflector.Referred(), ec->ConnectionId);
		ec->ConnectionId = UINT32_MAX;

		// errors with `attempt to modify readonly table`
		//lua_pushlightuserdata(L, L);
		//lua_pushnil(L);
		//lua_settable(L, LUA_ENVIRONINDEX); // remove stacktrace string

		ScriptEngine::L::StateUserdata* ud = (ScriptEngine::L::StateUserdata*)ec->L->userdata;
		if (ud->EventConnections.size() > 0)
		{
			const auto& it = std::find(ud->EventConnections.begin(), ud->EventConnections.end(), ec);
			assert(it != ud->EventConnections.end());
			ud->EventConnections.erase(it);
		}

		lua_unref(ec->L, ec->ThreadRef);
	}
	else
		luaL_error(L, "No such method of Event Connection known as '%s'", L->namecall->data);

	return 0;
}

static int conn_index(lua_State* L)
{
    const char* k = luaL_checkstring(L, 2);
	EventConnectionData* ec = (EventConnectionData*)luaL_checkudata(L, 1, "EventConnection");

	if (strcmp(k, "Connected") == 0)
		lua_pushboolean(L, ec->ConnectionId != UINT32_MAX);

	else if (strcmp(k, "Signal") == 0)
	{
		lua_pushinteger(L, ec->SignalRef);
		lua_gettable(L, LUA_REGISTRYINDEX);
	}
	else
		luaL_error(L, "Invalid member '%s' of Event Connection", k);

	return 1;
}

static int conn_tostring(lua_State* L)
{
    EventConnectionData* ec = (EventConnectionData*)luaL_checkudata(L, 1, "EventConnection");
	lua_pushinteger(L, ec->SignalRef);
	lua_gettable(L, LUA_REGISTRYINDEX);

	EventSignalData* ev = (EventSignalData*)luaL_checkudata(L, -1, "EventSignal");
	GameObject* obj = GameObject::GetObjectById(ev->Reflector.Id);

	std::string source = ev->Reflector.Type == EntityComponent::None
		? (obj ? obj->GetFullName() + "." : "GameObject::")
		: std::format("{}::", s_EntityComponentNames[(size_t)ev->Reflector.Type]);

    lua_pushfstring(L, "Connection to %s%s", source.c_str(), ev->EventName);
    return 1;
}

static void createmetatable(lua_State* L)
{
    luaL_newmetatable(L, "EventConnection");

    lua_pushstring(L, "EventConnection");
	lua_setfield(L, -2, "__type");

    lua_pushcfunction(L, conn_namecall, "__namecall");
	lua_setfield(L, -2, "__namecall");

	lua_pushcfunction(L, conn_index, "EventConnection.__index");
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, conn_tostring, "EventConnection.__tostring");
	lua_setfield(L, -2, "__tostring");

	lua_pop(L, 1);
}

int luhxopen_EventConnection(lua_State* L)
{
    createmetatable(L);
    return 0;
}
