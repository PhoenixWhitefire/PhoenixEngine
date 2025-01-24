#pragma once

// an assert, but also at runtime
#ifdef NDEBUG
#define PHX_ENSURE(res, err) if (!res) throw(err)
#else
#define PHX_ENSURE(expr, err) assert(expr)
#endif

#define PHX_CATCH_AND_RETHROW(c, expr_a, expr_b) catch (c Error) { std::string str = expr_a##Error##expr_b; throw(str); }

#include <string>

void CopyStringToBuffer(char* buf, size_t capacity, std::string string = "");
char* BufferInitialize(size_t capacity, const std::string& value = "");
double GetRunningTime();
