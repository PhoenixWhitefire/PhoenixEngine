#pragma once

#define RAISE_RT(errstring) throw(std::runtime_error(errstring))

// 24/01/2025
// an assert, but also at runtime
// can be useful, because important exceptions
// can end up being directed to the exception handlers
// in the `main` function, preventing VS debugger
// from being invoked directly at the point of the exception
#ifdef NDEBUG

#define PHX_ENSURE_MSG(expr, err) if (!(expr)) RAISE_RT(err)
#define PHX_ENSURE(expr) if (!(expr)) RAISE_RT("Failed to ensure: " + std::string(#expr))

#define PHX_CHECK(expr, err) if (!(expr)) Log::ErrorF( \
    "Check '{}' failed on line {} of function '{}'", \
    #expr, __LINE__, __FUNCTION__ \
); \

#else

#define PHX_ENSURE_MSG(expr, err) assert(expr)
#define PHX_ENSURE(expr) assert(expr)

#define PHX_CHECK assert

#endif

#include <format>
#include <assert.h>

#include "Log.hpp"

void CopyStringToBuffer(char* buf, size_t capacity, const std::string_view& string = "");
char* BufferInitialize(size_t capacity, const std::string_view& value = "");
double GetRunningTime();
