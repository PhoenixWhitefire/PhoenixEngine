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

    bool fileLoaded = true;
    std::string contents = FileRW::ReadFile(path, &fileLoaded);

    if (!fileLoaded)
        luaL_error(L, "%s", path, contents.c_str());

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

    lua_State* P = lua_newthread(bestVM->MainThread);
    int result = ScriptEngine::CompileAndLoad(P, contents, "@" + FileRW::ResolvePathNormalized(path));

    if (result != 0)
        luaL_error(L, "Error while loading %s: %s", path, lua_tostring(P, -1));

    result = ScriptEngine::L::Resume(P, nullptr, 0);
    if (result != LUA_OK && result != LUA_YIELD)
        luaL_error(L, "Parallel thread errored in initial step: %s", lua_tostring(P, -1));

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
