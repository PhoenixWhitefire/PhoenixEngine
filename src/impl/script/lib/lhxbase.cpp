#include <lualib.h>
#include <unordered_set>

#include "Reflection.hpp"
#include "lua.h"
#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "Version.hpp"
#include "Log.hpp"

#define MAXDEPTH (16)

static Reflection::GenericValue serialize_(lua_State* L, int Index, int Depth, std::unordered_set<const void*>& SeenTables)
{
    if (Depth > MAXDEPTH)
        return "[depth exceeded]";

    switch (lua_type(L, Index))
    {
    case LUA_TTABLE:
    {
        if (Depth > MAXDEPTH)
            return "[depth exceeded - table]";

        std::vector<Reflection::GenericValue> items;
        bool isArray = true;

        if (lua_getmetatable(L, Index))
        {
            isArray = false;

            items.emplace_back("(metatable)");
            items.push_back(serialize_(L, -1, Depth + 1, SeenTables));
            lua_pop(L, 1);

            size_t l = 0;
            const char* s = luaL_tolstring(L, -1, &l);

            items.emplace_back("(name)");
            items.emplace_back(std::string_view(s, l));
            lua_pop(L, 1);
        }

        int lastIndex = 0;

        // luau.org/api/#tables
        for (int iter = 0; (iter = lua_rawiter(L, Index, iter)) != -1;)
        {
            if (lua_type(L, -1) == LUA_TTABLE)
            {
                const void* table = lua_topointer(L, -1);

                if (SeenTables.find(table) != SeenTables.end())
                {
                    lua_pop(L, 1);
                    lua_pushliteral(L, "**[cycle]**");
                }
                else
                    SeenTables.insert(table);
            }

            int kt = lua_type(L, -2);

            if (isArray)
            {
                if (kt != LUA_TNUMBER)
                    isArray = false;
                else
                {
                    int index = lua_tointeger(L, -2);

                    bool isNonIntegerIndex = lua_tonumber(L, -2) != (double)index;
                    bool isOutOfOrderIndex = isNonIntegerIndex || (index < 1 || index != lastIndex + 1);

                    if (isNonIntegerIndex || isOutOfOrderIndex)
                        isArray = false;
                }

                if (isArray)
                    lastIndex = lua_tointeger(L, -2);
            }

            if (lua_type(L, -2) == LUA_TSTRING)
            {
                size_t l = 0;
                const char* s = lua_tolstring(L, -2, &l);

                if (s[0] == '(' || s[0] == '\\')
                {
                    std::string str = std::string(s, l);
                    str = "\\" + str;

                    int before = lua_gettop(L);

                    lua_remove(L, -2);
                    lua_pushlstring(L, str.data(), str.size());
                    lua_insert(L, -2);
                    lua_pop(L, 1);

                    assert(lua_gettop(L) == before);
                    (void)before;
                }
            }

            items.push_back(serialize_(L, -2, Depth + 1, SeenTables));
            items.push_back(serialize_(L, -1, Depth + 1, SeenTables));

            if (lua_type(L, -1) == LUA_TTABLE)
                SeenTables.erase(lua_topointer(L, -1));

            if (Depth >= MAXDEPTH)
                break;

            lua_pop(L, 2);
        }

        if (isArray)
        {
            std::vector<Reflection::GenericValue> array;
            array.reserve(items.size() / 2);

            for (uint32_t i = 1; i < items.size(); i += 2)
            {
                assert(items[i-1].Type == Reflection::ValueType::Double && items[i-1].AsDouble() == ((double)i + 1.0) / 2.0);
                array.push_back(items[i]);
            }

            return array;
        }
        else
        {
            Reflection::GenericValue map = items;
            map.Type = Reflection::ValueType::Map;

            return map;
        }
    }
    default:
        return ScriptEngine::L::ToGeneric(L, Index);
    }
}

static Reflection::GenericValue serialize(lua_State* L, int Index)
{
    std::unordered_set<const void*> seenTables;
    Reflection::GenericValue v;

    try
    {
        v = serialize_(L, Index, 0, seenTables);
    }
    catch (const std::exception& e)
    {
        size_t l = 0;
        const char* s = luaL_tolstring(L, Index, &l);

        v = Reflection::GenericValue(s);
        lua_pop(L, 1);

        Log.WarningF("Failed to serialize component {} of log message: {}", Index, e.what());
    }

    return v;
}

static std::string getScriptTraceExtraTags(lua_State* L)
{
    lua_Debug ar = {};
    lua_getinfo(L, 1, "sl", &ar);

    if (ar.short_src)
        return std::format("Script:{},Line:{}", ar.short_src, ar.currentline);
    else
        return "";
}

static void appendToLog(lua_State* L, Logging::MessageType Type)
{
    const std::string tags = getScriptTraceExtraTags(L);

    // FROM:
    // `luaB_print`
    // `Luau/VM/src/lbaselib.cpp`
    // 11/11/2024

    int n = lua_gettop(L); // number of arguments

    if (n == 1 && lua_type(L, 1) == LUA_TSTRING)
    {
        size_t l = 0;
        const char* s = luaL_tolstring(L, 1, &l);

        Log.Write(s, Type, tags);
        return;
    }

    std::vector<Reflection::GenericValue> values;

    for (int i = 1; i <= n; i++)
        values.push_back(serialize(L, i));

    Log.AppendWithValues(Type, "", values, tags);
}

static int base_print(lua_State* L)
{
    appendToLog(L, Logging::MessageType::Info);

    return 0;
}

static int base_warn(lua_State* L)
{
    appendToLog(L, Logging::MessageType::Warning);

    return 0;
}

static int base_appendlog(lua_State* L)
{
    appendToLog(L, Logging::MessageType::None);

    return 0;
}

static const luaL_Reg base_funcs[] =
{
    { "print", base_print },
    { "warn", base_warn },
    { "appendlog", base_appendlog },
    { NULL, NULL }
};

static void defineRuntime(lua_State* L)
{
    lua_createtable(L, 0, 3);

    lua_pushliteral(L, "phoenix");
    lua_setfield(L, -2, "name");

    lua_pushliteral(L, "https://github.com/PhoenixWhitefire/PhoenixEngine");
    lua_setfield(L, -2, "url");

    lua_createtable(L, 0, 2);

    lua_pushstring(L, GetEngineVersion());
    lua_setfield(L, -2, "display");

    lua_createtable(L, 0, 2);

    lua_pushliteral(L, "https://github.com/PhoenixWhitefire/PhoenixEngine");
    lua_setfield(L, -2, "url");

    lua_pushstring(L, GetEngineCommitHash());
    lua_setfield(L, -2, "commit");

    lua_setfield(L, -2, "git");

    lua_setfield(L, -2, "version");

    lua_setreadonly(L, -1, true);
    lua_setglobal(L, "_RUNTIME");
}

int luhxopen_base(lua_State* L)
{
    luaL_register(L, "_G", base_funcs);
    defineRuntime(L);

    return 1;
}
