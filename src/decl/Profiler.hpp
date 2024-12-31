#pragma once

#include <unordered_map>
#include <string>

// bullshit macro expansion semantics
#define PROFILE_SCOPE_INTERNAL2(label, paste) Profiler::TimeScope __profileScopeTimer##paste { label }
#define PROFILE_SCOPE_INTERNAL(label, paste) PROFILE_SCOPE_INTERNAL2(label, paste)
#define PROFILE_SCOPE(label) PROFILE_SCOPE_INTERNAL(label, __LINE__)

#define PROFILE_PROCEDURE(label, proc, ...) { PROFILE_SCOPE(label); proc(__VA_ARGS__); }
#define PROFILE_EXPRESSION(label, expr) { PROFILE_SCOPE(label); expr; }

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
