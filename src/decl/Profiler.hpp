#pragma once

#include <unordered_map>
#include <string>

// bullshit macro expansion semantics
#define PROFILER_PROFILE_SCOPE_ACTUAL(label, paste) Profiler::TimeScope __profileScopeTimer##paste { label }
#define PROFILER_PROFILE_SCOPE_2(label, paste) PROFILER_PROFILE_SCOPE_ACTUAL(label, paste)
#define PROFILER_PROFILE_SCOPE(label) PROFILER_PROFILE_SCOPE_2(label, __LINE__)

namespace Profiler
{
	void Start(const std::string& Label);
	void Stop();
	void Reset(const std::string& Label);
	void ResetAll();
	void PushSnapshot();
	std::unordered_map<std::string, double> PopSnapshot();

	class TimeScope
	{
	public:
		TimeScope(const std::string& Label);
		~TimeScope();

	private:
		TimeScope() = delete;
	};
}
