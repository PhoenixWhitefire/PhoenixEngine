#include <lualib.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "script/UserdataTags.hpp"

static void disconnect(EventConnectionData* ec)
{
    void* p = ec->Reflector.Referred();
    assert(p);

    ec->Event->Disconnect(p, ec->ConnectionId);

    // Cleanup was invoked by `::Disconnect`
}

static int conn_namecall(lua_State* L)
{
    if (strcmp(lua_namecallatom(L, nullptr), "Disconnect") == 0)
    {
        EventConnectionData* ec = (EventConnectionData*)luaL_checkudatatagged(L, 1, UserdataTag::EventConnection);

        if (ec->ConnectionId == UINT32_MAX)
            luaL_error(L, "Event Connection was already disconnected!");

        disconnect(ec);
    }
    else
        luaL_error(L, "No such method of Event Connection known as '%s'", lua_namecallatom(L, nullptr));

    return 0;
}

static int conn_index(lua_State* L)
{
    const char* k = luaL_checkstring(L, 2);
    EventConnectionData* ec = (EventConnectionData*)luaL_checkudatatagged(L, 1, UserdataTag::EventConnection);

    if (strcmp(k, "Connected") == 0)
        lua_pushboolean(L, ec->ConnectionId != UINT32_MAX);

    else if (strcmp(k, "Signal") == 0)
        lua_getref(L, ec->SignalRef);

    else
        luaL_error(L, "Invalid member '%s' of Event Connection", k);

    return 1;
}

static int conn_tostring(lua_State* L)
{
    EventConnectionData* ec = (EventConnectionData*)luaL_checkudatatagged(L, 1, UserdataTag::EventConnection);
    lua_getref(L, ec->SignalRef);

    EventSignalData* ev = (EventSignalData*)luaL_checkudatatagged(L, -1, UserdataTag::EventSignal);
    GameObject* obj = GameObjectManager::Get()->FindById(ev->Reflector.Id);

    std::string source = ev->Reflector.Type == EntityComponent::None
        ? (obj ? obj->GetFullName() + "." : "GameObject::")
        : std::format("{}::", s_EntityComponentNames[(size_t)ev->Reflector.Type]);

    lua_pushfstring(L, "Connection to %s%s", source.c_str(), ev->EventName);
    return 1;
}

static int conn_eq(lua_State* L)
{
    EventConnectionData* a = (EventConnectionData*)luaL_checkudatatagged(L, 1, UserdataTag::EventConnection);
    EventConnectionData* b = (EventConnectionData*)luaL_checkudatatagged(L, 2, UserdataTag::EventConnection);

    lua_pushboolean(
        L,
        a->Reflector == b->Reflector
            && a->ConnectionId == b->ConnectionId
            && a->Event == b->Event
    );
    return 1;
}

static void createmetatable(lua_State* L)
{
    lua_createtable(L, 0, 5);

    lua_pushliteral(L, "EventConnection");
    lua_setfield(L, -2, "__type");

    lua_pushcfunction(L, conn_namecall, "__namecall");
    lua_setfield(L, -2, "__namecall");

    lua_pushcfunction(L, conn_index, "EventConnection.__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, conn_tostring, "EventConnection.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, conn_eq, "EventConnection.__eq");
    lua_setfield(L, -2, "__eq");

    lua_pushliteral(L, "The metatable is locked");
	lua_setfield(L, -2, "__metatable");

    lua_setuserdatametatable(L, UserdataTag::EventConnection);

    lua_setuserdatadtor(L, UserdataTag::EventConnection, [](lua_State*, void* ud)
    {
        EventConnectionData* ec = (EventConnectionData*)ud;

        if (ec->ConnectionId != UINT32_MAX)
            disconnect(ec);
    });
}

int luhxopen_EventConnection(lua_State* L)
{
    createmetatable(L);
    return 0;
}
