// TracyLuau.hpp, 25/06/2026

#pragma once

#include <lua.h>
#include <lualib.h>
#include <assert.h>
#include <limits>

#include "tracy/public/common/TracyColor.hpp"
#include "tracy/public/common/TracyAlign.hpp"
#include "tracy/public/common/TracyForceInline.hpp"
#include "tracy/public/common/TracySystem.hpp"
#include "tracy/public/client/TracyProfiler.hpp"

// 24/06/2026
// Ported from `tracy/public/TracyLua.hpp`
namespace tracy
{

#ifdef TRACY_ON_DEMAND
TRACY_API LuaZoneState& GetLuaZoneState();
#endif

static inline void LuauShortenSrc( char* dst, const char* src )
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
    LuauShortenSrc( src, dbg.source );
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
    LuauShortenSrc( src, dbg.source );
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

static inline int LuauZoneBeginNImpl( lua_State* L, std::string* ZoneName )
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
    LuauShortenSrc( src, dbg.source );

    uint64_t srcloc = UINT64_MAX;

    if (lua_isnoneornil(L, 1))
    {
        const char* name = dbg.name ? dbg.name : dbg.short_src;

        srcloc = Profiler::AllocSourceLocation( dbg.currentline, src, name );

        if (ZoneName)
            *ZoneName = name;
    }
    else
    {
        size_t nsz = 0;
        const auto name = lua_tolstring(L, 1, &nsz);
        srcloc = Profiler::AllocSourceLocation( dbg.currentline, src, dbg.name ? dbg.name : dbg.short_src, name, nsz );

        if (ZoneName)
            *ZoneName = name;
    }

    TracyQueuePrepare( QueueType::ZoneBeginAllocSrcLoc );
    MemWrite( &item->zoneBegin.time, Profiler::GetTime() );
    MemWrite( &item->zoneBegin.srcloc, srcloc );
    TracyQueueCommit( zoneBeginThread );
    return 0;
#endif
}

static inline void LuauZoneEndImpl()
{
#ifdef TRACY_ON_DEMAND
    assert( GetLuaZoneState().counter != 0 );
    GetLuaZoneState().counter--;
    if( !GetLuaZoneState().active )
        return;
    if( !GetProfiler().IsConnected() )
    {
        GetLuaZoneState().active = false;
        return;
    }
#endif

    TracyQueuePrepare( QueueType::ZoneEnd );
    MemWrite( &item->zoneEnd.time, Profiler::GetTime() );
    TracyQueueCommit( zoneEndThread );
}

static inline int LuauZoneEnd( lua_State* )
{
    LuauZoneEndImpl();
    return 0;
}

static inline int LuauZoneText( lua_State* L )
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

#if 0
static inline int LuauZoneName( lua_State* L )
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
#endif

static inline int LuauMessage( lua_State* L )
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
        detail::LuauShortenSrc( src, ar->short_src );

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
