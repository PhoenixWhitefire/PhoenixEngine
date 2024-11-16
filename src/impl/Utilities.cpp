#include <chrono>

#include "src/decl/Utilities.hpp"

static auto s_ChronoStartTime = std::chrono::high_resolution_clock::now();

void CopyStringToBuffer(char* buf, size_t capacity, const std::string& string)
{
	for (size_t i = 0; i < capacity; i++)
		buf[i] = i < string.size() ? string[i] : 0;
}

char* BufferInitialize(size_t capacity, const std::string& value)
{
	char* buf = (char*)malloc(capacity);

	if (!buf)
		throw("There are bigger problems at hand.");

	CopyStringToBuffer(buf, capacity, value);

	return buf;
}

double GetRunningTime()
{
	auto chronoTime = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::nanoseconds>(chronoTime - s_ChronoStartTime).count() / 1e+9;
}
