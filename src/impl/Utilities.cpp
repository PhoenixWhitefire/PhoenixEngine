#include <chrono>

#include "Utilities.hpp"
#include "Memory.hpp"

static auto s_ChronoStartTime = std::chrono::high_resolution_clock::now();

double GetRunningTime()
{
	auto chronoTime = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::nanoseconds>(chronoTime - s_ChronoStartTime).count() / 1e+9;
}
