// Parallelization, 23/05/2026
#include <lualib.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "ThreadManager.hpp"
#include "FileRW.hpp"

static void createParallelVMs()
{
    ThreadManager* threadManager = ThreadManager::Get();
    ScriptEngine::ParallelVMs.reserve(threadManager->Concurrency);

    for (int i = 0; i < threadManager->Concurrency; i++)
        ScriptEngine::CreateParallelVM();
}

static int parallel_spawn(lua_State* L)
{
    ScriptEngine::L::StateUserdata* vmud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(L));

    if (vmud->PVM && vmud->PVM->Desynchronized)
        luaL_error(L, "Cannot spawn parallel threads while desynchronized");

    const char* path = luaL_checkstring(L, 1);

    if (ScriptEngine::ParallelVMs.empty())
        createParallelVMs();

    ScriptEngine::ParallelVM* bestVM = nullptr;

    for (ScriptEngine::ParallelVM* parallelVM : ScriptEngine::ParallelVMs)
    {
        if (!bestVM || parallelVM->ParallelAllocated < bestVM->ParallelAllocated)
            bestVM = parallelVM;
    }

    if (!bestVM)
        luaL_error(L, "Parallelization is not available");

    std::vector<Reflection::GenericValue> arguments;
    arguments.reserve(lua_gettop(L) - 1);

    for (int i = 2; i <= lua_gettop(L); i++)
        arguments.push_back(ScriptEngine::L::ToGeneric(L, i));

    std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(bestVM->ParallelSpawnRequestsMutex);
    bestVM->ParallelSpawnRequests.emplace_back(path, arguments);
    bestVM->ParallelAllocated++;

    return 0;
}

static bool isVMSynchronized(ScriptEngine::L::StateUserdata* vmud)
{
    return !vmud->PVM || !vmud->PVM->Desynchronized;
}

static int parallel_synchronized(lua_State* L)
{
    ScriptEngine::L::StateUserdata* vmud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(L));
    lua_pushboolean(L, isVMSynchronized(vmud));

    return 1;
}

static int parallel_synchronize(lua_State* L)
{
    ScriptEngine::L::StateUserdata* vmud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(L));

    if (!vmud->PVM)
        luaL_error(L, "Tried to synchronize a non-parallel VM");

    if (isVMSynchronized(vmud))
        return 0;

    return ScriptEngine::L::Yield(
        L,
        0,
        [](ScriptEngine::YieldedCoroutine& yc)
        {
            yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Deferred;
        },
        &vmud->PVM->YieldedCoroutinesSync
    );
}

static int parallel_desynchronize(lua_State* L)
{
    ScriptEngine::L::StateUserdata* vmud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(L));

    if (!vmud->PVM)
        luaL_error(L, "Tried to desynchronize a non-parallel VM");

    if (!isVMSynchronized(vmud))
        return 0;

    return ScriptEngine::L::Yield(
        L,
        0,
        [](ScriptEngine::YieldedCoroutine& yc)
        {
            yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Deferred;
        }
    );
}

static const luaL_Reg parallel_funcs[] = {
    { "spawn", parallel_spawn },
    { "synchronized", parallel_synchronized },
    { "synchronize", parallel_synchronize },
    { "desynchronize", parallel_desynchronize },

    { NULL, NULL }
};

int luhxopen_parallel(lua_State* L)
{
    luaL_register(L, LUHX_PARALLELLIBNAME, parallel_funcs);

    return 1;
}
