#include <SDL2/SDL_timer.h>

#include "ThreadManager.hpp"

ThreadManager* ThreadManager::Singleton = new ThreadManager();

static void _workerTaskRunner(Worker* ThisWorker)
{
	while (true)
	{
		if (ThisWorker->TaskQueue.empty())
		{
			ThisWorker->Status = WorkerStatus::WaitingForTask;

			SDL_Delay(50); // TODO could probably have a better method for giving the CPU time to do other stuff

			continue;
		}

		Task* currentTask = ThisWorker->TaskQueue.back();

		ThisWorker->Status = WorkerStatus::RunningTask;

		currentTask->Function(currentTask->FuncArgument);

		ThisWorker->TaskIdx++;

		delete currentTask;

		ThisWorker->TaskQueue.pop_back();
	}
}

void Worker::WaitForCurrentTaskFinish() const
{
	uint32_t lastTask = this->TaskIdx;

	while ((lastTask == this->TaskIdx) || (this->Status != WorkerStatus::RunningTask))
		SDL_Delay(2);
}

void Worker::WaitForAllTasksFinish() const
{
	while (this->Status != WorkerStatus::WaitingForTask)
		SDL_Delay(2);
}

std::vector<Worker*> ThreadManager::GetWorkers()
{
	return m_Workers;
}

std::vector<Worker*> ThreadManager::CreateWorkers(int NumWorkers, WorkerType Type)
{
	std::vector<Worker*> newWorkers;
	newWorkers.reserve(NumWorkers);

	for (int index = 0; index < NumWorkers; index++)
	{
		Worker* newWorker = new Worker();
		newWorker->Type = Type;

		newWorker->Thread = new std::thread(&_workerTaskRunner, newWorker);
		newWorker->Thread->detach();

		m_Workers.push_back(newWorker);
		newWorkers.push_back(newWorker);
	}

	return newWorkers;
}

void ThreadManager::DispatchJob(Task& Job)
{
	Worker* assignedWorker = nullptr;

	for (Worker* worker : m_Workers)
	{
		if (worker->Type == WorkerType::DefaultTaskWorker)
		{
			if (worker->Status == WorkerStatus::WaitingForTask)
				assignedWorker = worker;
			else
			{
				if (!assignedWorker || (worker->TaskQueue.size() < assignedWorker->TaskQueue.size()))
					assignedWorker = worker;
			}
		}
	}

	if (!assignedWorker)
	{
		//OnErrorEvent.Fire("Could not DispatchJob, number of workers may be too low or none are DefaultTaskWorker.");

		throw("Could not DispatchJob, there might not be any workers or none are a DefaultTaskWorker");

		return;
	}

	// got a worker, tell it to do the task
	assignedWorker->TaskQueue.push_back(&Job);
}

ThreadManager* ThreadManager::Get()
{
	if (!ThreadManager::Singleton)
		throw("ThreadManager::Get was called before Engine was initialized.");

	return ThreadManager::Singleton;
}
