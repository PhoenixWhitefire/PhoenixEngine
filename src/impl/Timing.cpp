#include <cfloat>
#include <stack>

#include "Timing.hpp"
#include "Utilities.hpp"

static std::stack<uint8_t> s_TimerStack;

void Timing::BeginTimer(uint8_t Timer)
{
	// RECURSION NOT GOOD
	assert(Timing::StartTimes[Timer] == FLT_MAX);

	s_TimerStack.push(Timer);
	Timing::StartTimes[Timer] = GetRunningTime();
}

void Timing::EndLastTimer()
{
	assert(s_TimerStack.size() > 0);

	uint8_t timer = s_TimerStack.top();
	s_TimerStack.pop();

	Timing::AccumulatedTimes[timer] += GetRunningTime() - Timing::StartTimes[timer];
	Timing::StartTimes[timer] = FLT_MAX;
}

void Timing::Finish()
{
	// `::Finish` will be called prior to the scopes for the
	// `EntireFrame` and `FrameWork` timers ending
	Timing::EndLastTimer();
	Timing::EndLastTimer();
	assert(s_TimerStack.size() == 0);

	// Timing::FinalFrameTimes = Timing::AccumulatedTimes;
	for (uint8_t i = 0; i < UINT8_MAX; i++)
		FinalFrameTimes[i] = AccumulatedTimes[i];

	for (uint8_t i = 0; i < UINT8_MAX; i++)
		AccumulatedTimes[i] = 0.f;
}

Timing::StaticMagicTimerThing::StaticMagicTimerThing(const char* Name)
{
	TimerId = s_NumTimers;

	TimerNames[TimerId] = Name;
	StartTimes[TimerId] = FLT_MAX;
	s_NumTimers++;
}

Timing::ScopedTimer::ScopedTimer(uint8_t Timer)
{
	Timing::BeginTimer(Timer);
}

Timing::ScopedTimer::~ScopedTimer()
{
	// refer to the comment inside `::Finish`
	if (s_TimerStack.size() > 0)
		Timing::EndLastTimer();
}
