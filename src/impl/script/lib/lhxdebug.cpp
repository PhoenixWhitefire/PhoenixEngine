#include <lualib.h>
#include <assert.h>
#include <limits>

#include "tracy/public/common/TracyColor.hpp"
#include "tracy/public/common/TracyAlign.hpp"
#include "tracy/public/common/TracyForceInline.hpp"
#include "tracy/public/common/TracySystem.hpp"
#include "tracy/public/client/TracyProfiler.hpp"

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"

// FROM: `ldblib.cpp` 15/08/2025
static lua_State* getthread(lua_State* L, int* arg)
{
    if (lua_isthread(L, 1))
    {
        *arg = 1;
        return lua_tothread(L, 1);
    }
    else
    {
        *arg = 0;
        return L;
    }
}

static int debug_traceback(lua_State* L)
{
    int arg;
    lua_State* L1 = getthread(L, &arg);
    const char* msg = luaL_optstring(L, arg + 1, NULL);
    int level = luaL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    luaL_argcheck(L, level >= 0, arg + 2, "level can't be negative");

	std::string trace;
	ScriptEngine::L::DumpStacktrace(L, &trace, level, msg);

	lua_pushlstring(L, trace.data(), trace.size());
	return 1;
}

// FROM: `luau/tests/Conformance.test.cpp` line 1543 for `breakpoint` function
static int debug_breakpoint(lua_State* L)
{
	if (lua_gettop(L) == 0)
		return lua_break(L);

	int line = luaL_checkinteger(L, 1);
    bool enabled = luaL_optboolean(L, 2, true);

    lua_Debug ar = {};
    lua_getinfo(L, lua_stackdepth(L) - 1, "f", &ar);

    lua_pushinteger(L, lua_breakpoint(L, -1, line, enabled));
    return 1;
}

// 24/06/2026
// Ported from `tracy/public/TracyLua.hpp`
namespace tracy
{

#ifdef TRACY_ON_DEMAND
TRACY_API LuaZoneState& GetLuaZoneState();
#endif

namespace detail
{

static inline void LuaShortenSrc( char* dst, const char* src )
{
    size_t l = std::min( (size_t)255, strlen( src ) );
    for( size_t i=0; i<l; i++ )
    {
        if( src[i] == '\n' ) dst[i] = ' ';
        else dst[i] = src[i];
    }
    dst[l] = 0;
}

#if 0
#ifdef TRACY_HAS_CALLSTACK
static tracy_force_inline void SendLuaCallstack( lua_State* L, uint32_t depth )
{
    assert( depth <= 64 );
    lua_Debug dbg[64] = {};
    const char* func[64];
    uint32_t fsz[64];
    uint32_t ssz[64];

    uint8_t cnt;
    uint16_t spaceNeeded = sizeof( cnt );
    for( cnt=0; cnt<depth; cnt++ )
    {
        if( lua_getinfo( L, cnt + 1, "snl", dbg+cnt ) == 0 ) break;
        func[cnt] = dbg[cnt].name ? dbg[cnt].name : dbg[cnt].short_src;
        fsz[cnt] = uint32_t( strlen( func[cnt] ) );
        ssz[cnt] = uint32_t( strlen( dbg[cnt].source ) );
        spaceNeeded += fsz[cnt] + ssz[cnt];
    }
    spaceNeeded += cnt * ( 4 + 2 + 2 );     // source line, function string length, source string length

    auto ptr = (char*)tracy_malloc( spaceNeeded + 2 );
    auto dst = ptr;
    memcpy( dst, &spaceNeeded, 2 ); dst += 2;
    memcpy( dst, &cnt, 1 ); dst++;
    for( uint8_t i=0; i<cnt; i++ )
    {
        const uint32_t line = dbg[i].currentline;
        memcpy( dst, &line, 4 ); dst += 4;
        assert( fsz[i] <= (std::numeric_limits<uint16_t>::max)() );
        memcpy( dst, fsz+i, 2 ); dst += 2;
        memcpy( dst, func[i], fsz[i] ); dst += fsz[i];
        assert( ssz[i] <= (std::numeric_limits<uint16_t>::max)() );
        memcpy( dst, ssz+i, 2 ); dst += 2;
        memcpy( dst, dbg[i].source, ssz[i] ), dst += ssz[i];
    }
    assert( dst - ptr == spaceNeeded + 2 );

    TracyQueuePrepare( QueueType::CallstackAlloc );
    MemWrite( &item->callstackAllocFat.ptr, (uint64_t)ptr );
    MemWrite( &item->callstackAllocFat.nativePtr, (uint64_t)Callstack( depth ) );
    TracyQueueCommit( callstackAllocFatThread );
}

static inline int LuaZoneBeginS( lua_State* L )
{
#ifdef TRACY_ON_DEMAND
    const auto zoneCnt = GetLuaZoneState().counter++;
    if( zoneCnt != 0 && !GetLuaZoneState().active ) return 0;
    GetLuaZoneState().active = GetProfiler().IsConnected();
    if( !GetLuaZoneState().active ) return 0;
#endif

#if defined TRACY_CALLSTACK && TRACY_CALLSTACK > 0
    const uint32_t depth = TRACY_CALLSTACK;
#else
    const auto depth = uint32_t( lua_tointeger( L, 1 ) );
#endif
    assert( depth > 0 ); // Would crash later anyway, this is not allowed
    SendLuaCallstack( L, depth );

    lua_Debug dbg = {};
    lua_getinfo( L, 1, "snl", &dbg );
    char src[256];
    LuaShortenSrc( src, dbg.source );
    const auto srcloc = Profiler::AllocSourceLocation( dbg.currentline, src, dbg.name ? dbg.name : dbg.short_src );

    TracyQueuePrepare( QueueType::ZoneBeginAllocSrcLocCallstack );
    MemWrite( &item->zoneBegin.time, Profiler::GetTime() );
    MemWrite( &item->zoneBegin.srcloc, srcloc );
    TracyQueueCommit( zoneBeginThread );

    return 0;
}

static inline int LuaZoneBeginNS( lua_State* L )
{
#ifdef TRACY_ON_DEMAND
    const auto zoneCnt = GetLuaZoneState().counter++;
    if( zoneCnt != 0 && !GetLuaZoneState().active ) return 0;
    GetLuaZoneState().active = GetProfiler().IsConnected();
    if( !GetLuaZoneState().active ) return 0;
#endif

#if defined TRACY_CALLSTACK && TRACY_CALLSTACK > 0
    const uint32_t depth = TRACY_CALLSTACK;
#else
    const auto depth = uint32_t( lua_tointeger( L, 2 ) );
#endif
    assert( depth > 0 ); // Would crash later anyway, this is not allowed
    SendLuaCallstack( L, depth );

    lua_Debug dbg = {};
    lua_getinfo( L, 1, "snl", &dbg );
    size_t nsz;
    char src[256];
    LuaShortenSrc( src, dbg.source );
    const auto name = lua_tolstring( L, 1, &nsz );
    const auto srcloc = Profiler::AllocSourceLocation( dbg.currentline, src, dbg.name ? dbg.name : dbg.short_src, name, nsz );

    TracyQueuePrepare( QueueType::ZoneBeginAllocSrcLocCallstack );
    MemWrite( &item->zoneBegin.time, Profiler::GetTime() );
    MemWrite( &item->zoneBegin.srcloc, srcloc );
    TracyQueueCommit( zoneBeginThread );

    return 0;
}
#endif
#endif

static inline int LuaZoneBeginN( lua_State* L )
{
#if defined TRACY_HAS_CALLSTACK && defined TRACY_CALLSTACK && TRACY_CALLSTACK > 0
    return LuaZoneBeginNS( L );
#else
#ifdef TRACY_ON_DEMAND
    const auto zoneCnt = GetLuaZoneState().counter++;
    if( zoneCnt != 0 && !GetLuaZoneState().active ) return 0;
    GetLuaZoneState().active = GetProfiler().IsConnected();
    if( !GetLuaZoneState().active ) return 0;
#endif

    lua_Debug dbg = {};
    lua_getinfo( L, 1, "snl", &dbg );
    char src[256];
    LuaShortenSrc( src, dbg.source );

    uint64_t srcloc = UINT64_MAX;

    if (lua_isnoneornil(L, 1))
    {
        srcloc = Profiler::AllocSourceLocation( dbg.currentline, src, dbg.name ? dbg.name : dbg.short_src );
    }
    else
    {
        size_t nsz = 0;
        const auto name = lua_tolstring(L, 1, &nsz);
        srcloc = Profiler::AllocSourceLocation( dbg.currentline, src, dbg.name ? dbg.name : dbg.short_src, name, nsz );
    }

    TracyQueuePrepare( QueueType::ZoneBeginAllocSrcLoc );
    MemWrite( &item->zoneBegin.time, Profiler::GetTime() );
    MemWrite( &item->zoneBegin.srcloc, srcloc );
    TracyQueueCommit( zoneBeginThread );
    return 0;
#endif
}

static inline int LuaZoneEnd( lua_State* )
{
#ifdef TRACY_ON_DEMAND
    assert( GetLuaZoneState().counter != 0 );
    GetLuaZoneState().counter--;
    if( !GetLuaZoneState().active ) return 0;
    if( !GetProfiler().IsConnected() )
    {
        GetLuaZoneState().active = false;
        return 0;
    }
#endif

    TracyQueuePrepare( QueueType::ZoneEnd );
    MemWrite( &item->zoneEnd.time, Profiler::GetTime() );
    TracyQueueCommit( zoneEndThread );
    return 0;
}

static inline int LuaZoneText( lua_State* L )
{
#ifdef TRACY_ON_DEMAND
    if( !GetLuaZoneState().active ) return 0;
    if( !GetProfiler().IsConnected() )
    {
        GetLuaZoneState().active = false;
        return 0;
    }
#endif
    luaL_checktype(L, 1, LUA_TSTRING);

    auto txt = lua_tostring( L, 1 );
    const auto size = strlen( txt );
    assert( size < (std::numeric_limits<uint16_t>::max)() );

    auto ptr = (char*)tracy_malloc( size );
    memcpy( ptr, txt, size );

    TracyQueuePrepare( QueueType::ZoneText );
    MemWrite( &item->zoneTextFat.text, (uint64_t)ptr );
    MemWrite( &item->zoneTextFat.size, (uint16_t)size );
    TracyQueueCommit( zoneTextFatThread );
    return 0;
}

static inline int LuaZoneName( lua_State* L )
{
#ifdef TRACY_ON_DEMAND
    if( !GetLuaZoneState().active ) return 0;
    if( !GetProfiler().IsConnected() )
    {
        GetLuaZoneState().active = false;
        return 0;
    }
#endif
    luaL_checktype(L, 1, LUA_TSTRING);

    auto txt = lua_tostring( L, 1 );
    const auto size = strlen( txt );
    assert( size < (std::numeric_limits<uint16_t>::max)() );

    auto ptr = (char*)tracy_malloc( size );
    memcpy( ptr, txt, size );

    TracyQueuePrepare( QueueType::ZoneName );
    MemWrite( &item->zoneTextFat.text, (uint64_t)ptr );
    MemWrite( &item->zoneTextFat.size, (uint16_t)size );
    TracyQueueCommit( zoneTextFatThread );
    return 0;
}

static inline int LuaMessage( lua_State* L )
{
#ifdef TRACY_ON_DEMAND
    if( !GetProfiler().IsConnected() ) return 0;
#endif
    luaL_checktype(L, 1, LUA_TSTRING);

    auto txt = lua_tostring( L, 1 );
    const auto size = strlen( txt );
    assert( size < (std::numeric_limits<uint16_t>::max)() );

    auto ptr = (char*)tracy_malloc( size );
    memcpy( ptr, txt, size );

    TaggedUserlandAddress taggedPtr{ (uint64_t)ptr, MakeMessageMetadata( MessageSourceType::User, MessageSeverity::Info ) };

    TracyQueuePrepare( QueueType::Message );
    MemWrite( &item->messageFat.time, Profiler::GetTime() );
    MemWrite( &item->messageFat.textAndMetadata, taggedPtr );
    MemWrite( &item->messageFat.size, (uint16_t)size );
    TracyQueueCommit( messageFatThread );
    return 0;
}

}

static inline void LuauRegister( lua_State* L )
{
    assert(lua_istable(L, -1));

    lua_pushcfunction( L, detail::LuaZoneBeginN, "profilebegin" );
    lua_setfield( L, -2, "profilebegin" );
#if 0
#ifdef TRACY_HAS_CALLSTACK
    lua_pushcfunction( L, detail::LuaZoneBeginS );
    lua_setfield( L, -2, "ZoneBeginS" );
    lua_pushcfunction( L, detail::LuaZoneBeginNS );
    lua_setfield( L, -2, "ZoneBeginNS" );
#else
    lua_pushcfunction( L, detail::LuaZoneBegin );
    lua_setfield( L, -2, "ZoneBeginS" );
    lua_pushcfunction( L, detail::LuaZoneBeginN );
    lua_setfield( L, -2, "ZoneBeginNS" );
#endif
#endif
    lua_pushcfunction( L, detail::LuaZoneEnd, "profileend" );
    lua_setfield( L, -2, "profileend" );
    lua_pushcfunction( L, detail::LuaZoneText, "profilezonetext" );
    lua_setfield( L, -2, "profilezonetext" );
    lua_pushcfunction( L, detail::LuaZoneName, "profilezonename" );
    lua_setfield( L, -2, "profilezonename" );
    lua_pushcfunction( L, detail::LuaMessage, "profilemessage" );
    lua_setfield( L, -2, "profilemessage" );
}

#if 0
static inline void LuaHook( lua_State* L, lua_Debug* ar )
{
    if ( ar->event == LUA_HOOKCALL )
    {
#ifdef TRACY_ON_DEMAND
        const auto zoneCnt = GetLuaZoneState().counter++;
        if ( zoneCnt != 0 && !GetLuaZoneState().active ) return;
        GetLuaZoneState().active = GetProfiler().IsConnected();
        if ( !GetLuaZoneState().active ) return;
#endif
        lua_getinfo( L, "Snl", ar );

        char src[256];
        detail::LuaShortenSrc( src, ar->short_src );

        const auto srcloc = Profiler::AllocSourceLocation( ar->currentline, src, ar->name ? ar->name : ar->short_src );
        TracyQueuePrepare( QueueType::ZoneBeginAllocSrcLoc );
        MemWrite( &item->zoneBegin.time, Profiler::GetTime() );
        MemWrite( &item->zoneBegin.srcloc, srcloc );
        TracyQueueCommit( zoneBeginThread );
    }
    else if (ar->event == LUA_HOOKRET) {
#ifdef TRACY_ON_DEMAND
        assert( GetLuaZoneState().counter != 0 );
        GetLuaZoneState().counter--;
        if ( !GetLuaZoneState().active ) return;
        if ( !GetProfiler().IsConnected() )
        {
            GetLuaZoneState().active = false;
            return;
        }
#endif
        TracyQueuePrepare( QueueType::ZoneEnd );
        MemWrite( &item->zoneEnd.time, Profiler::GetTime() );
        TracyQueueCommit( zoneEndThread );
    }
}
#endif

}

const luaL_Reg xdebug_funcs[] = {
    { "traceback", debug_traceback },
    { "breakpoint", debug_breakpoint },
    { NULL, NULL }
};

int luhxopen_debug(lua_State* L)
{
    luaL_register(L, LUA_DBLIBNAME, xdebug_funcs);
    tracy::LuauRegister(L); // profilebegin, profileend, profilezonename, profilezonetext, profilemessage

    return 1;
}
