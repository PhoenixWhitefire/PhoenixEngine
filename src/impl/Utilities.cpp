#include <chrono>

#include "Utilities.hpp"
#include "Memory.hpp"

static auto s_ChronoStartTime = std::chrono::high_resolution_clock::now();

void CopyStringToBuffer(char* buf, size_t capacity, const std::string_view& string)
{
	for (size_t i = 0; i < capacity; i++)
		buf[i] = i < string.length() ? string.at(i) : 0;
}

char* BufferInitialize(size_t capacity, const std::string_view& value)
{
	char* buf = (char*)Memory::Alloc(capacity + 1);

	if (!buf)
		RAISE_RTF("Failed to allocate {} bytes in BufferInitialize", capacity + 1);

	buf[capacity] = 0;

	CopyStringToBuffer(buf, capacity, value);

	return buf;
}

double GetRunningTime()
{
	auto chronoTime = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::nanoseconds>(chronoTime - s_ChronoStartTime).count() / 1e+9;
}
