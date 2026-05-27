// Parallelization, 23/05/2026
#include <lualib.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "script/UserdataTags.hpp"
#include "ThreadManager.hpp"
#include "Memory.hpp"
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

struct SharedMutex
{
    std::atomic_int ReferenceCount;
    std::mutex Mutex;
};

struct SharedBuffer
{
    std::atomic_int ReferenceCount;
    void* Data = nullptr;
    size_t Size = 0;
};

static std::unordered_map<std::string, SharedMutex*> ParallelMutexes;
static std::mutex ParallelMutexesMutex;

static std::unordered_map<std::string, SharedBuffer*> ParallelBuffers;
static std::mutex ParallelBuffersMutex;

// supports both number and integer
static size_t checkSizeArgument(lua_State* L, int Index)
{
    if (lua_isnumber(L, Index))
    {
        double n = luaL_checknumber(L, 2);

        if (n < 1.f || std::floor(n) != n)
            luaL_error(L, "Size argument (%lf) cannot be zero, negative, or fractional", n);

        return (size_t)n;
    }
    else if (lua_isinteger64(L, 2))
    {
        int64_t i = luaL_checkinteger64(L, 2);

        if (i < 1)
            luaL_error(L, "Size argument (%zi) cannot be zero or negative", i);

        return (size_t)i;
    }
    else
        luaL_typeerror(L, Index, "number or integer");
}

static int parallelmutex_namecall(lua_State* L)
{
    SharedMutex* mutex = (SharedMutex*)lua_touserdatatagged(L, 1, UserdataTag::Mutex);
    if (!mutex)
        luaL_error(L, "No metatable chicanery :)");

    const char* k = lua_namecallatom(L, nullptr);

    if (strcmp(k, "Lock") == 0)
        mutex->Mutex.lock();
    else if (strcmp(k, "Unlock") == 0)
        mutex->Mutex.unlock();
    else if (strcmp(k, "TryLock") == 0)
    {
        lua_pushboolean(L, mutex->Mutex.try_lock());
        return 1;
    }
    else
        luaL_error(L, "Invalid method '%s'", k);

    return 0;
}

static int sharedbuffer_index(lua_State* L)
{
    SharedBuffer* buffer = *(SharedBuffer**)lua_touserdatatagged(L, 1, UserdataTag::SharedBuffer);
    if (!buffer)
        luaL_error(L, "No metatable chicanery :)");

    const char* k = luaL_checkstring(L, 2);

    if (strcmp(k, "Size") == 0)
        lua_pushnumber(L, (double)buffer->Size);
    else
        luaL_error(L, "Invalid member '%s'", k);

    return 1;
}

static int sharedbuffer_namecall(lua_State* L)
{
    SharedBuffer* buffer = *(SharedBuffer**)lua_touserdatatagged(L, 1, UserdataTag::SharedBuffer);
    if (!buffer)
        luaL_error(L, "No metatable chicanery :)");

    const char* k = lua_namecallatom(L, nullptr);

    if (strcmp(k, "Pull") == 0)
    {
        size_t start = lua_isnoneornil(L, 2) ? 0 : checkSizeArgument(L, 2);
        size_t length = lua_isnoneornil(L, 3) ? buffer->Size - start : checkSizeArgument(L, 3);

        if (buffer->Size < start + length)
            luaL_error(L, "Slice exceeds bounds of the Shared Buffer");

        void* lb = lua_newbuffer(L, length);
        memcpy(lb, (void*)((size_t)buffer->Data + start), length);

        return 1;
    }
    else if (strcmp(k, "Push") == 0)
    {
        size_t lblen = 0;
        void* lb = luaL_checkbuffer(L, 2, &lblen);

        size_t start = lua_isnoneornil(L, 3) ? 0 : checkSizeArgument(L, 3);
        size_t length = lua_isnoneornil(L, 4) ? lblen : checkSizeArgument(L, 4);

        if (buffer->Size < start + length)
            luaL_error(L, "Slice exceeds bound of the Shared Buffer");

        memcpy((void*)((size_t)buffer->Data + start), lb, length);
        return 0;
    }
    else
        luaL_error(L, "Invalid method '%s'", k);
}

static void pushSharedMutex(lua_State* L, SharedMutex* Mutex)
{
    Mutex->ReferenceCount++;

    void* ud = lua_newuserdatataggedwithmetatable(L, sizeof(SharedMutex*), UserdataTag::Mutex);
    memcpy(ud, &Mutex, sizeof(void*));
}

static void pushSharedBuffer(lua_State* L, SharedBuffer* Buffer)
{
    Buffer->ReferenceCount++;

    void* ud = lua_newuserdatataggedwithmetatable(L, sizeof(SharedBuffer*), UserdataTag::SharedBuffer);
    memcpy(ud, &Buffer, sizeof(void*));
}

static int parallel_mutex(lua_State* L)
{
    size_t nlen = 0;
    const char* cstr = luaL_checklstring(L, 1, &nlen);
    std::string name = { cstr, nlen };

    SharedMutex* mtx = nullptr;
    std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(ParallelMutexesMutex);

    if (const auto& it = ParallelMutexes.find(name); it != ParallelMutexes.end())
        mtx = it->second;
    else
    {
        mtx = new SharedMutex;

        ParallelMutexes[name] = mtx;
    }

    pushSharedMutex(L, mtx);
    return 1;
}

static int parallel_sharedbuffer(lua_State* L)
{
    size_t nlen = 0;
    const char* cstr = luaL_checklstring(L, 1, &nlen);
    std::string name = { cstr, nlen };

    size_t size = checkSizeArgument(L, 2);

    SharedBuffer* buffer = nullptr;
    std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(ParallelBuffersMutex);

    if (const auto& it = ParallelBuffers.find(name); it != ParallelBuffers.end())
    {
        if (it->second->Size != size)
            luaL_error(L, "Size of %zi bytes for shared buffer with ID '%s' does not match previous instantiation of %zi bytes", size, name.c_str(), it->second->Size);

        buffer = it->second;
    }
    else
    {
        buffer = new SharedBuffer;
        buffer->Size = size;
        buffer->Data = Memory::Alloc(size);

        if (!buffer->Data)
            throw std::bad_alloc();

        ParallelBuffers[name] = buffer;
    }

    pushSharedBuffer(L, buffer);
    return 1;
}

static const luaL_Reg parallel_funcs[] = {
    { "spawn", parallel_spawn },
    { "synchronized", parallel_synchronized },
    { "synchronize", parallel_synchronize },
    { "desynchronize", parallel_desynchronize },

    { "mutex", parallel_mutex },
    { "sharedbuffer", parallel_sharedbuffer },

    { NULL, NULL }
};

int luhxopen_parallel(lua_State* L)
{
    lua_setuserdatadtor(L, UserdataTag::Mutex, [](lua_State*, void* ud)
    {
        SharedMutex* mtx = (SharedMutex*)ud;
        mtx->ReferenceCount--;
    });

    lua_createtable(L, 0, 2);

    lua_pushcfunction(L, parallelmutex_namecall, "__namecall"); // leave as SPECIFICALLY `__namecall` for better stack tracebacks
    lua_setfield(L, -2, "__namecall");

    lua_pushliteral(L, "Mutex");
    lua_setfield(L, -2, "__type");

    lua_setuserdatametatable(L, UserdataTag::Mutex);

    lua_setuserdatadtor(L, UserdataTag::SharedBuffer, [](lua_State*, void* ud)
    {
        SharedBuffer* mtx = (SharedBuffer*)ud;
        mtx->ReferenceCount--;
    });

    lua_createtable(L, 0, 3);

    lua_pushcfunction(L, sharedbuffer_index, "SharedBuffer.__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, sharedbuffer_namecall, "__namecall"); // leave as SPECIFICALLY `__namecall` for better stack tracebacks
    lua_setfield(L, -2, "__namecall");

    lua_pushliteral(L, "SharedBuffer");
    lua_setfield(L, -2, "__type");

    lua_setuserdatametatable(L, UserdataTag::SharedBuffer);

    luaL_register(L, LUHX_PARALLELLIBNAME, parallel_funcs);
    return 1;
}
