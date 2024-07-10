#pragma once

#include<functional>
#include<string>
#include<thread>

enum class WorkerStatus { WaitingForTask, RunningTask };

// SpecialWorkers will not get assigned tasks by DispatchJob
enum class WorkerType { DefaultTaskWorker, SpecialWorker };

struct Task
{
	std::function<void(void*)> Function;
	void* FuncArgument = nullptr;
	std::string DbgInfo;
};

class Worker
{
public:
	std::thread* Thread = nullptr;

	// Yield the calling thread for the worker to finish the task it's currently on.
	// Will not yield if the worker isn't performing a task (in WaitingForTask state)
	void WaitForCurrentTaskFinish();
	// Yield the calling thread for the worker to finish all it's tasks.
	void WaitForAllTasksFinish();

	std::vector<Task*> TaskQueue;
	
	// Worker goes into WaitingForTask if it has exhausted it's task queue.
	WorkerStatus Status = WorkerStatus::WaitingForTask;
	WorkerType Type = WorkerType::DefaultTaskWorker;

	// The number of tasks it has completed. Used by WaitForCurrentTaskFinish to judge if it has moved on to a different task.
	// Is incremented after it completes a task.
	uint32_t TaskIdx;
};

class ThreadManager
{
public:
	ThreadManager();

	std::vector<Worker*> CreateWorkers(int NumWorkers, WorkerType Type);
	std::vector<Worker*> GetWorkers();

	// give a task to a free worker, or one that has the least tasks
	void DispatchJob(Task& Job);

	static ThreadManager* Get();

	SDL_Window* ApplicationWindow;

private:
	static ThreadManager* Singleton;

	std::vector<Worker*> m_Workers;
};
