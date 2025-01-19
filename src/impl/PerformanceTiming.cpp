#include <assert.h>
#include <stack>

#include "PerformanceTiming.hpp"
#include "Utilities.hpp"

static std::stack<Timing::Timer> s_TimerStack;

void Timing::BeginTimer(Timing::Timer Timer)
{
	s_TimerStack.push(Timer);
	Timing::StartTimes[(uint8_t)Timer] = GetRunningTime();
}

void Timing::EndLastTimer()
{
	Timer timer = s_TimerStack.top();
	uint8_t timerId = (uint8_t)timer;
	s_TimerStack.pop();

	Timing::AccumulatedTimes[timerId] += GetRunningTime() - Timing::StartTimes[timerId];
}

void Timing::Finish()
{
	// `::Finish` will be called prior to the `EntireFrame` scope actually
	// end, let's just end it here 19/02/2025
	Timing::EndLastTimer();
	assert(s_TimerStack.size() == 0);
	
	Timing::FinalFrameTimes = Timing::AccumulatedTimes;
	Timing::AccumulatedTimes.fill(0.f);
}

Timing::TimeWithRaii::TimeWithRaii(Timing::Timer Timer)
{
	Timing::BeginTimer(Timer);
}

Timing::TimeWithRaii::~TimeWithRaii()
{
	// refer to the comment inside `::Finish`
	if (s_TimerStack.size() > 0)
		Timing::EndLastTimer();
}
