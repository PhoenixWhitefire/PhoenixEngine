#include <chrono>

#include "src/decl/Utilities.hpp"

static auto s_ChronoStartTime = std::chrono::high_resolution_clock::now();

void CopyStringToBuffer(char* buf, size_t capacity, std::string string)
{
	string.resize(capacity - 2);
	memcpy(buf, string.c_str(), capacity - 2);

	//buf[std::min(capacity, string.length())] = 0;
}

char* BufferInitialize(size_t capacity, const std::string& value)
{
	char* buf = (char*)malloc(capacity + 1);

	if (!buf)
		throw("There are bigger problems at hand.");

	buf[capacity] = 0;

	CopyStringToBuffer(buf, capacity, value);

	return buf;
}

double GetRunningTime()
{
	auto chronoTime = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::nanoseconds>(chronoTime - s_ChronoStartTime).count() / 1e+9;
}
