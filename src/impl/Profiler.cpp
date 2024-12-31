#include <chrono>
#include <stack>

#include "Profiler.hpp"
#include "Utilities.hpp"

static std::unordered_map<std::string, double> s_Timings{};
static std::unordered_map<std::string, double> s_StartTimes{};

static std::stack<std::string> s_PreviousLabels{};
static std::stack<std::unordered_map<std::string, double>> s_Snapshots{};

void Profiler::Start(const std::string& Label)
{
	std::string trackedLabel;

	if (!s_PreviousLabels.empty())
		trackedLabel = s_PreviousLabels.top() + "/" + Label;
	else
		trackedLabel = Label;

	if (s_StartTimes.find(trackedLabel) != s_StartTimes.end())
		throw("Tried to start timing for label " + trackedLabel + " but it was already running");

	s_StartTimes[trackedLabel] = GetRunningTime();
	s_PreviousLabels.push(trackedLabel);
}

void Profiler::Stop()
{
	std::string label = s_PreviousLabels.top();
	s_PreviousLabels.pop();

	if (s_StartTimes.find(label) == s_StartTimes.end())
		throw("Tried to stop timing for label " + label + ", but nothing had started it");

	double prevValue = s_Timings.find(label) != s_Timings.end() ? s_Timings[label] : 0.f;

	s_Timings[label] = prevValue + (GetRunningTime() - s_StartTimes[label]);
	s_StartTimes.erase(label);
}

void Profiler::Reset(const std::string& Label)
{
	s_Timings[Label] = 0.f;
	
	if (s_StartTimes.find(Label) != s_StartTimes.end())
		s_StartTimes.erase(Label);
}

void Profiler::ResetAll()
{
	for (auto& it : s_Timings)
		s_Timings[it.first] = 0.f;

	s_StartTimes.clear();
	s_PreviousLabels = {};
}

void Profiler::PushSnapshot()
{
	s_Snapshots.push(s_Timings);
}

std::unordered_map<std::string, double> Profiler::PopSnapshot()
{
	if (s_Snapshots.size() == 0)
		return {};

	std::unordered_map<std::string, double> v = s_Snapshots.top();
	s_Snapshots.pop();

	return v;
}

Profiler::TimeScope::TimeScope(const std::string& Label)
{
	Profiler::Start(Label);
}

Profiler::TimeScope::~TimeScope()
{
	Profiler::Stop();
}
