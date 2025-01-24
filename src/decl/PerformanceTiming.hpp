#pragma once

#include <stdint.h>
#include <array>

#define TIME_SCOPE_AS_INTERNAL2(Timer, Line) Timing::TimeWithRaii __scopetimer_##Line(Timer);
#define TIME_SCOPE_AS_INTERNAL(Timer, Line) TIME_SCOPE_AS_INTERNAL2(Timer, Line)
#define TIME_SCOPE_AS(Timer) TIME_SCOPE_AS_INTERNAL(Timer, __LINE__)

namespace Timing
{
	enum class Timer : uint8_t
	{
		EntireFrame,
		Rendering,
		Physics,
		Scripts,
		__count
	};

	void BeginTimer(Timing::Timer);
	void EndLastTimer();
	// mark end of the frame
	void Finish();

	// when each timer started
	inline std::array<double, (size_t)Timer::__count> StartTimes;
	// the length of time each timer took
	inline std::array<double, (size_t)Timer::__count> AccumulatedTimes;
	// after an entire frame, `AccumulatedTimes` is copied over into this
	// and reset
	inline std::array<double, (size_t)Timer::__count> FinalFrameTimes;

	class TimeWithRaii
	{
	public:
		TimeWithRaii(Timing::Timer Timer);
		~TimeWithRaii();
	};

	static inline const char* TimerNames[] =
	{
		"EntireFrame",
		"Rendering",
		"Physics",
		"Scripts"
	};
	
	static_assert(std::size(TimerNames) == static_cast<size_t>(Timer::__count));
}
