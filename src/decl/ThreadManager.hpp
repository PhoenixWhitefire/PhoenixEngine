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
	void Dispatch(std::function<void()>);
	void PropagateExceptions();

	static ThreadManager* Get();

private:
	struct Worker
	{
		std::thread Thread;
		std::exception_ptr Exception;
	};

	std::vector<Worker> m_Workers;

	std::queue<std::function<void()>> m_Tasks;
	std::mutex m_TasksMutex;
	std::condition_variable m_TasksCv;

	bool m_Stop = false;
};
