#include <chrono>

#include "Utilities.hpp"
#include "Memory.hpp"

static auto s_ChronoStartTime = std::chrono::high_resolution_clock::now();

void CopyStringToBuffer(char* buf, uint32_t capacity, const std::string_view& string)
{
	for (uint32_t i = 0; i < capacity; i++)
		buf[i] = i < string.length() ? string.at(i) : 0;

	buf[capacity - 1] = 0;
}

char* BufferInitialize(uint32_t capacity, const std::string_view& value)
{
	char* buf = (char*)Memory::Alloc(capacity);

	if (!buf)
		RAISE_RTF("Failed to allocate {} bytes in BufferInitialize", capacity + 1);

	buf[capacity - 1] = 0;
	CopyStringToBuffer(buf, capacity, value);

	return buf;
}

double GetRunningTime()
{
	auto chronoTime = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::nanoseconds>(chronoTime - s_ChronoStartTime).count() / 1e+9;
}
