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
    {
        std::string name = "ParallelVM" + std::to_string(i);
        lua_State* L = ScriptEngine::L::Create(name);

        ScriptEngine::ParallelVMs.push_back(ScriptEngine::LuauVM{
            .Name = name,
            .MainThread = L,
            .IsParallel = true,
        });
    }
}

static int parallel_spawn(lua_State* L)
{
    if (!ScriptEngine::L::IsSynchronized(L))
        luaL_error(L, "Cannot spawn parallel threads while desynchronized");

    const char* path = luaL_checkstring(L, 1);

    ThreadManager* threadManager = ThreadManager::Get();

    if (ScriptEngine::ParallelVMs.empty())
        createParallelVMs();

    ScriptEngine::LuauVM* bestVM = nullptr;

    for (ScriptEngine::LuauVM& parallelVM : ScriptEngine::ParallelVMs)
    {
        if (!bestVM || parallelVM.ParallelAllocated < bestVM->ParallelAllocated)
            bestVM = &parallelVM;
    }

    if (!bestVM)
        luaL_error(L, "Parallelization is not available");

    std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(bestVM->ParallelSpawnRequestsMutex);
    bestVM->ParallelSpawnRequests.push_back(path);

    return 0;
}

static const luaL_Reg parallel_funcs[] = {
    { "spawn", parallel_spawn },

    { NULL, NULL }
};

int luhxopen_parallel(lua_State* L)
{
    luaL_register(L, LUHX_PARALLELLIBNAME, parallel_funcs);

    return 1;
}
