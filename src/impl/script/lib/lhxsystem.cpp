// lhxsystem.cpp, 29/08/2026
#include <lualib.h>
#include <cstdlib>
#include <thread>

#ifdef __clang__
#include <sys/sysinfo.h>
#else
#include <Windows.h>
#endif

#include "script/luhx.hpp"

static int system_hostname(lua_State* L)
{
#ifdef _WIN32
    const char* envVar = "COMPUTERNAME"
#else
    const char* envVar = "HOSTNAME";
#endif

    const char* hostname = std::getenv(envVar);

    lua_pushstring(L, hostname ? hostname : "unknown");
    return 1;
}

static int system_threadcount(lua_State* L)
{
    lua_pushunsigned(L, std::thread::hardware_concurrency());
    return 1;
}

static int system_totalmemory(lua_State* L)
{
    size_t total = 0;

#ifdef _WIN32
    MEMORYSTATUSEX memInfo = {};
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memInfo))
        total = memInfo.ullTotalPhys;
#else
    struct sysinfo si = {};

    if (sysinfo(&si) == 0)
        total = (size_t)si.totalram * si.mem_unit;
#endif

    lua_pushnumber(L, (double)total);
    return 1;
}

static int system_freememory(lua_State* L)
{
    size_t free = 0;

#ifdef _WIN32
    MEMORYSTATUSEX memInfo = {};
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memInfo))
        free = memInfo.ullAvailPhys;
#else
    struct sysinfo si = {};

    if (sysinfo(&si) == 0)
        free = (size_t)si.freeram * si.mem_unit;
#endif

    lua_pushnumber(L, (double)free);
    return 1;
}

luaL_Reg system_funcs[] = {
    { "hostname", system_hostname },
    { "threadcount", system_threadcount },
    { "totalmemory", system_totalmemory },
    { "freememory", system_freememory },
    { NULL, NULL }
};

int luhxopen_system(lua_State* L)
{
    luaL_register(L, LUHX_SYSTEMLIBNAME, system_funcs);
    return 1;
}
