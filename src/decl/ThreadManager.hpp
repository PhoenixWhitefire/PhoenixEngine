#pragma once

#include <condition_variable>
#include <functional>
#include <string>
#include <thread>
#include <queue>
#include <mutex>

class ThreadManager
{
public:
	void Shutdown();
	// USE SHUTDOWN!!
	~ThreadManager();

	void Initialize(int NumThreads = -1);

	// queue a task
	// "Critical" means "we can't skip this if we're shutting down"
	// we skip non-critical tasks while shutting down to speed up
	// how fast the window closes
	// window waits on everything to finish so that the app does not
	// appear like a suspicious background process if it gets frozen
	// somewhere in teardown
	void Dispatch(std::function<void()>, bool IsCritical);

	static ThreadManager* Get();

private:
	struct Task
	{
		std::function<void()> Function;
		bool IsCritical = true;
	};

	void m_StopThreads();

	std::vector<std::jthread> m_Workers;

	std::queue<Task> m_Tasks;
	std::mutex m_TasksMutex;
	std::condition_variable m_TasksCv;

	bool m_Stop = false;
};
