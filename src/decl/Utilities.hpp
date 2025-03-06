#pragma once

// 24/01/2025
// an assert, but also at runtime
// can be useful, because important exceptions
// can end up being directed to the exception handlers
// in the `main` function, preventing VS debugger
// from being invoked directly at the point of the exception
// wait this actually makes no fucking sense hold on
#ifdef NDEBUG
#define PHX_ENSURE_MSG(expr, err) if (!(expr)) throw(err)
#define PHX_ENSURE(expr) if (!(expr)) throw("Check failed: " + std::string(#expr))
#else
#define PHX_ENSURE_MSG(expr, err) assert(expr)
#define PHX_ENSURE(expr, err) assert(expr)
#endif

#include <string>

void CopyStringToBuffer(char* buf, size_t capacity, const std::string_view& string = "");
char* BufferInitialize(size_t capacity, const std::string_view& value = "");
double GetRunningTime();
