// DebugBreakReason.hpp, 07/06/2026
#pragma once

struct DebugBreakReason_
{
    enum DBR {
        BrokeIntoDebugger, // `lua_break` or generic reason for thread entering `LUA_BREAK` state
        Breakpoint,        // `lua_breakpoint`
        Interrupt,         // `interruptThread` of `coresumey` in `coroutine.resume` implementation
        Error,             // error message expected to be at the top of the stack
        DebuggerStep       // debugger is stepping through the code
    };
};

using DebugBreakReason = DebugBreakReason_::DBR;
