#pragma once

#include<string>
#include<thread>

#include<SDL2/SDL_timer.h>
#include<SDL2/SDL_messagebox.h>

#include<datatype/Event.hpp>

enum class WorkerStatus { WaitingForTask, RunningTask };

// SpecialWorkers will not get assigned tasks by DispatchJob
enum class WorkerType { DefaultTaskWorker, SpecialWorker };

class Task
{
public:
	std::function<void(void*)> Function;
	void* FuncArgument = nullptr;
	std::string DbgInfo;
};

class Worker
{
public:
	std::thread* Thread = nullptr;

	//Yield the calling thread for the worker to finish the task it's currently on. Will not yield if the worker isn't performing a task (in WaitingForTask state)
	void WaitForCurrentTaskFinish();
	//Yield the calling thread for the worker to finish all it's tasks.
	void WaitForAllTasksFinish();

	std::vector<Task*> TaskQueue;
	
	//NOTE:	Worker only goes into WaitingForTask if it has exhausted it's task queue.
	WorkerStatus Status = WorkerStatus::WaitingForTask;
	WorkerType Type = WorkerType::DefaultTaskWorker;

	// The number of tasks it has completed. Used by WaitForCurrentTaskFinish to judge if it has moved on to a different task.
	// Is incremented after it completes a task.
	unsigned int TaskIdx;
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
	static ThreadManager* Singleton;

	SDL_Window* ApplicationWindow;

private:
	std::vector<Worker*> m_Workers;
};
