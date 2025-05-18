#pragma once

#include <stdint.h>
#include <array>

#define __TIME_SCOPE_AS_INTERNAL(StrName, Line) \
static Timing::StaticMagicTimerThing __timingmagic_##Line(StrName); \
Timing::ScopedTimer __timingscoped_##Line((__timingmagic_##Line).TimerId);

#define TIME_SCOPE_AS(StrName) __TIME_SCOPE_AS_INTERNAL(StrName, __LINE__)

#define TIME_SCOPE_AS_N(StrName, VarName) \
static Timing::StaticMagicTimerThing VarName(StrName); \
Timing::ScopedTimer __timingscoped_for_##VarName(VarName.TimerId); \

namespace Timing
{
	void BeginTimer(uint8_t);
	void EndLastTimer();
	// mark end of the frame
	void Finish();

	// when each timer started
	inline double StartTimes[UINT8_MAX] = { 0.f };
	// the length of time each timer took
	inline double AccumulatedTimes[UINT8_MAX] = { 0.f };
	// after an entire frame, `AccumulatedTimes` is copied over into this
	// and reset
	inline double FinalFrameTimes[UINT8_MAX] = { 0.f };

	inline const char* TimerNames[UINT8_MAX] = { NULL };

	struct StaticMagicTimerThing
	{
		StaticMagicTimerThing(const char* Name);

		static inline uint8_t s_NumTimers = 0;
		uint8_t TimerId = UINT8_MAX;
	};

	struct ScopedTimer
	{
		ScopedTimer(uint8_t);
		~ScopedTimer();
	};
}
