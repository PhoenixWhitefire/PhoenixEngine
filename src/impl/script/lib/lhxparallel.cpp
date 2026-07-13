// Parallelization, 23/05/2026
#include <tracy/Tracy.hpp>
#include <lualib.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "script/UserdataTags.hpp"
#include "script/SharedMutex.hpp"
#include "ThreadManager.hpp"
#include "Memory.hpp"
#include "FileRW.hpp"

static void createParallelVMs()
{
    ThreadManager* threadManager = ThreadManager::Get();
    ScriptEngine::ParallelVMs.reserve(threadManager->Concurrency);

    for (int i = 0; i < threadManager->Concurrency; i++)
        ScriptEngine::CreateParallelVM();

    if (threadManager->Concurrency == 0)
        ScriptEngine::CreateParallelVM(); // at least one
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

struct SharedBuffer
{
    void* Data = nullptr;
    size_t Size = 0;
    std::atomic_int ReferenceCount;
};

struct SharedAtomicInt
{
    std::atomic_int Int;
    std::atomic_int ReferenceCount;
};

static std::unordered_map<std::string, SharedMutex*> ParallelMutexes;
static std::mutex ParallelMutexesMutex;

static std::unordered_map<std::string, SharedBuffer*> ParallelBuffers;
static std::mutex ParallelBuffersMutex;

static std::unordered_map<std::string, SharedAtomicInt*> ParallelAtomicInts;
static std::mutex ParallelAtomicIntsMutex;

// supports both number and integer
static size_t checkSizeArgument(lua_State* L, int Index, const char* Arg, bool AllowZero = false)
{
    if (lua_isnumber(L, Index))
    {
        double n = luaL_checknumber(L, Index);

        if (AllowZero)
        {
            if (n < 0.f || std::floor(n) != n)
                luaL_error(L, "%s argument #%i (%lf) cannot be negative or fractional", Arg, Index, n);
        }
        else
        {
            if (n < 1.f || std::floor(n) != n)
                luaL_error(L, "%s argument #%i (%lf) cannot be zero, negative, or fractional", Arg, Index, n);
        }

        return (size_t)n;
    }
    else if (lua_isinteger64(L, Index))
    {
        int64_t i = luaL_checkinteger64(L, Index);

        if (AllowZero)
        {
            if (i < 0)
                luaL_error(L, "%s argument #%i (%zi) cannot be negative or above int64 maximum", Arg, Index, i);
        }
        else
        {
            if (i < 1)
                luaL_error(L, "%s argument #%i (%zi) cannot be zero, negative, or above int64 maximum", Arg, Index, i);
        }

        return (size_t)i;
    }
    else
        luaL_typeerror(L, Index, "number or integer");
}

static ScriptEngine::LuauVM* getLuauVMFromVMUD(ScriptEngine::L::StateUserdata* vmud)
{
    return vmud->PVM ? vmud->PVM : &ScriptEngine::VMs.at(vmud->VM);
}

static int parallelmutex_namecall(lua_State* L)
{
    ZoneScoped;

    ScriptEngine::L::StateUserdata* vmud = (ScriptEngine::L::StateUserdata*)lua_getthreaddata(lua_mainthread(L));
    ScriptEngine::LuauVM* lvm = getLuauVMFromVMUD(vmud);

    SharedMutex** sharedMutexp = (SharedMutex**)lua_touserdatatagged(L, 1, UserdataTag::Mutex);
    if (!sharedMutexp)
        luaL_error(L, "No metatable chicanery :)");

    SharedMutex* sharedMutex = *sharedMutexp;

    auto alreadyLockedIt = std::find(lvm->LockedSharedMutexes.begin(), lvm->LockedSharedMutexes.end(), sharedMutex);

    const char* k = lua_namecallatom(L, nullptr);
    ZoneText(k, strlen(k));

    if (strcmp(k, "Lock") == 0)
    {
        if (alreadyLockedIt == lvm->LockedSharedMutexes.end())
        {
            sharedMutex->Mutex.lock();
            lvm->LockedSharedMutexes.push_back(sharedMutex);
        }
        // do not report error if it was already locked, multiple parallel tasks can run on the same VM!
    }
    else if (strcmp(k, "Unlock") == 0)
    {
        if (alreadyLockedIt != lvm->LockedSharedMutexes.end())
        {
            sharedMutex->Mutex.unlock();
            lvm->LockedSharedMutexes.erase(alreadyLockedIt);
        }
        else
            luaL_error(L, "Tried to unlock shared mutex that was not locked by this VM");
    }
    else if (strcmp(k, "TryLock") == 0)
    {
        if (alreadyLockedIt == lvm->LockedSharedMutexes.end())
        {
            bool success = sharedMutex->Mutex.try_lock();
            lua_pushboolean(L, success);

            if (success)
                lvm->LockedSharedMutexes.push_back(sharedMutex);
        }
        else
            lua_pushboolean(L, true); // already locked

        return 1;
    }
    else
        luaL_error(L, "Invalid method '%s'", k);

    return 0;
}

static int sharedbuffer_index(lua_State* L)
{
    SharedBuffer** bufferp = (SharedBuffer**)lua_touserdatatagged(L, 1, UserdataTag::SharedBuffer);
    if (!bufferp)
        luaL_error(L, "No metatable chicanery :)");

    SharedBuffer* buffer = *bufferp;

    const char* k = luaL_checkstring(L, 2);

    if (strcmp(k, "Size") == 0)
        lua_pushnumber(L, (double)buffer->Size);
    else
        luaL_error(L, "Invalid member '%s'", k);

    return 1;
}

static int sharedbuffer_namecall(lua_State* L)
{
    ZoneScoped;

    SharedBuffer** bufferp = (SharedBuffer**)lua_touserdatatagged(L, 1, UserdataTag::SharedBuffer);
    if (!bufferp)
        luaL_error(L, "No metatable chicanery :)");

    SharedBuffer* buffer = *bufferp;

    const char* k = lua_namecallatom(L, nullptr);
    ZoneText(k, strlen(k));

    if (strcmp(k, "Pull") == 0)
    {
        size_t start = lua_isnoneornil(L, 2) ? 0 : checkSizeArgument(L, 2, "Start", true);
        size_t length = lua_isnoneornil(L, 3) ? buffer->Size - start : checkSizeArgument(L, 3, "Length");

        if (buffer->Size < start + length)
            luaL_error(L, "Slice at offset %zi with length %zi exceeds bounds of the shared buffer by %zi byte(s)", start, length, start + length - buffer->Size);

        void* lb = lua_newbuffer(L, length);
        memcpy(lb, (void*)((size_t)buffer->Data + start), length);

        return 1;
    }
    else if (strcmp(k, "Push") == 0)
    {
        size_t lblen = 0;
        void* lb = luaL_checkbuffer(L, 2, &lblen);

        size_t start = lua_isnoneornil(L, 3) ? 0 : checkSizeArgument(L, 3, "Start", true);
        size_t length = lua_isnoneornil(L, 4) ? lblen : checkSizeArgument(L, 4, "Length");

        if (buffer->Size < start + length)
            luaL_error(L, "Slice at offset %zi with length %zi exceeds bounds of the shared buffer by %zi byte(s)", start, length, start + length - buffer->Size);

        memcpy((void*)((size_t)buffer->Data + start), lb, length);
        return 0;
    }
    else
        luaL_error(L, "Invalid method '%s'", k);
}

static int atomicint_namecall(lua_State* L)
{
    ZoneScoped;

    SharedAtomicInt** sintp = (SharedAtomicInt**)lua_touserdatatagged(L, 1, UserdataTag::AtomicInteger);
    if (!sintp)
        luaL_error(L, "No metatable chicanery :)");

    SharedAtomicInt* sint = *sintp;

    const char* k = lua_namecallatom(L, nullptr);
    ZoneText(k, strlen(k));

    if (strcmp(k, "Get") == 0)
    {
        lua_pushinteger(L, sint->Int);
        return 1;
    }
    else if (strcmp(k, "Set") == 0)
    {
        sint->Int = luaL_checkinteger(L, 2);
        return 0;
    }
    else
        luaL_error(L, "Invalid method '%s'", k);
}

#define PARALLEL_REG "PARALLEL"

static void pushSharedMutex(lua_State* L, SharedMutex* Mutex)
{
    lua_getfield(L, LUA_REGISTRYINDEX, PARALLEL_REG);
    if (lua_isnil(L, -1))
    {
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, PARALLEL_REG);
    }

    lua_pushlightuserdata(L, Mutex);
    lua_gettable(L, -2); // PARALLEL_REG[lud]

    if (!lua_isnil(L, -1))
    {
        lua_remove(L, -2); // remove the registry sub-table
        return; // object already in the registry, return the same value (to make table key hashing/rawequal work)
    }
    lua_pop(L, 1); // dont need that nil

    Mutex->ReferenceCount.fetch_add(1, std::memory_order_relaxed);

    void* ud = lua_newuserdatataggedwithmetatable(L, sizeof(SharedMutex*), UserdataTag::Mutex);
    lua_pushlightuserdata(L, Mutex);
    lua_pushvalue(L, -2);

    memcpy(ud, &Mutex, sizeof(SharedMutex*));

    lua_settable(L, -4);
    lua_remove(L, -2); // remove the registry sub-table
}

static void pushSharedBuffer(lua_State* L, SharedBuffer* Buffer)
{
    lua_getfield(L, LUA_REGISTRYINDEX, PARALLEL_REG);
    if (lua_isnil(L, -1))
    {
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, PARALLEL_REG);
    }

    lua_pushlightuserdata(L, Buffer);
    lua_gettable(L, -2); // PARALLEL_REG[lud]

    if (!lua_isnil(L, -1))
    {
        lua_remove(L, -2); // remove the registry sub-table
        return; // object already in the registry, return the same value (to make table key hashing/rawequal work)
    }
    lua_pop(L, 1); // dont need that nil

    Buffer->ReferenceCount.fetch_add(1, std::memory_order_relaxed);

    void* ud = lua_newuserdatataggedwithmetatable(L, sizeof(SharedBuffer*), UserdataTag::SharedBuffer);
    lua_pushlightuserdata(L, Buffer);
    lua_pushvalue(L, -2);

    memcpy(ud, &Buffer, sizeof(SharedBuffer*));

    lua_settable(L, -4);
    lua_remove(L, -2); // remove the registry sub-table
}

static void pushAtomicInt(lua_State* L, SharedAtomicInt* AtomicInt)
{
    lua_getfield(L, LUA_REGISTRYINDEX, PARALLEL_REG);
    if (lua_isnil(L, -1))
    {
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, PARALLEL_REG);
    }

    lua_pushlightuserdata(L, AtomicInt);
    lua_gettable(L, -2); // PARALLEL_REG[lud]

    if (!lua_isnil(L, -1))
    {
        lua_remove(L, -2); // remove the registry sub-table
        return; // object already in the registry, return the same value (to make table key hashing/rawequal work)
    }
    lua_pop(L, 1); // dont need that nil

    AtomicInt->ReferenceCount.fetch_add(1, std::memory_order_relaxed);

    void* ud = lua_newuserdatataggedwithmetatable(L, sizeof(SharedAtomicInt*), UserdataTag::AtomicInteger);
    lua_pushlightuserdata(L, AtomicInt);
    lua_pushvalue(L, -2);

    memcpy(ud, &AtomicInt, sizeof(SharedAtomicInt*));

    lua_settable(L, -4);
    lua_remove(L, -2); // remove the registry sub-table
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
        mtx->Name = name;

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

    size_t size = checkSizeArgument(L, 2, "Size");

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
        memset(buffer->Data, 0, size);

        if (!buffer->Data)
            throw std::bad_alloc();

        ParallelBuffers[name] = buffer;
    }

    pushSharedBuffer(L, buffer);
    return 1;
}

static int parallel_atomicinteger(lua_State* L)
{
    size_t nlen = 0;
    const char* cstr = luaL_checklstring(L, 1, &nlen);
    std::string name = { cstr, nlen };

    SharedAtomicInt* sint = nullptr;
    std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(ParallelAtomicIntsMutex);

    if (const auto& it = ParallelAtomicInts.find(name); it != ParallelAtomicInts.end())
        sint = it->second;
    else
    {
        sint = new SharedAtomicInt;
        sint->Int = luaL_optinteger(L, 2, 0);

        ParallelAtomicInts[name] = sint;
    }

    pushAtomicInt(L, sint);
    return 1;
}

static const luaL_Reg parallel_funcs[] = {
    { "spawn", parallel_spawn },
    { "synchronized", parallel_synchronized },
    { "synchronize", parallel_synchronize },
    { "desynchronize", parallel_desynchronize },

    { "mutex", parallel_mutex },
    { "sharedbuffer", parallel_sharedbuffer },
    { "atomicinteger", parallel_atomicinteger },

    { NULL, NULL }
};

int luhxopen_parallel(lua_State* L)
{
    lua_setuserdatadtor(L, UserdataTag::Mutex, [](lua_State*, void* ud)
    {
        SharedMutex* mtx = *(SharedMutex**)ud;
        mtx->ReferenceCount.fetch_sub(1, std::memory_order_relaxed);
    });

    lua_createtable(L, 0, 2);

    lua_pushcfunction(L, parallelmutex_namecall, "__namecall"); // leave as SPECIFICALLY `__namecall` for better stack tracebacks
    lua_setfield(L, -2, "__namecall");

    lua_pushliteral(L, "Mutex");
    lua_setfield(L, -2, "__type");

    lua_setuserdatametatable(L, UserdataTag::Mutex);

    lua_setuserdatadtor(L, UserdataTag::SharedBuffer, [](lua_State*, void* ud)
    {
        SharedBuffer* mtx = *(SharedBuffer**)ud;
        mtx->ReferenceCount.fetch_sub(1, std::memory_order_relaxed);
    });

    lua_createtable(L, 0, 3);

    lua_pushcfunction(L, sharedbuffer_index, "SharedBuffer.__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, sharedbuffer_namecall, "__namecall"); // leave as SPECIFICALLY `__namecall` for better stack tracebacks
    lua_setfield(L, -2, "__namecall");

    lua_pushliteral(L, "SharedBuffer");
    lua_setfield(L, -2, "__type");

    lua_setuserdatametatable(L, UserdataTag::SharedBuffer);

    lua_setuserdatadtor(L, UserdataTag::AtomicInteger, [](lua_State*, void* ud)
    {
        SharedAtomicInt* sint = *(SharedAtomicInt**)ud;
        sint->ReferenceCount.fetch_sub(1, std::memory_order_relaxed);
    });

    lua_createtable(L, 0, 2);

    lua_pushcfunction(L, atomicint_namecall, "__namecall"); // leave as SPECIFICALLY `__namecall` for better stack tracebacks
    lua_setfield(L, -2, "__namecall");

    lua_pushliteral(L, "AtomicInteger");
    lua_setfield(L, -2, "__type");

    lua_setuserdatametatable(L, UserdataTag::AtomicInteger);

    luaL_register(L, LUHX_PARALLELLIBNAME, parallel_funcs);
    return 1;
}

void CollectParallelResourceGarbage()
{
    ZoneScoped;

    for (auto it = ParallelMutexes.begin(); it != ParallelMutexes.end();)
    {
        if (it->second->ReferenceCount == 0)
        {
            delete it->second;
            it = ParallelMutexes.erase(it);
        }
        else
            it++;
    }

    for (auto it = ParallelBuffers.begin(); it != ParallelBuffers.end();)
    {
        if (it->second->ReferenceCount == 0)
        {
            Memory::Free(it->second->Data);
            delete it->second;

            it = ParallelBuffers.erase(it);
        }
        else
            it++;
    }

    for (auto it = ParallelAtomicInts.begin(); it != ParallelAtomicInts.end();)
    {
        if (it->second->ReferenceCount == 0)
        {
            delete it->second;
            it = ParallelAtomicInts.erase(it);
        }
        else
            it++;
    }
}
