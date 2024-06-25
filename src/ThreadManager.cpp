#include<SDL2/SDL_timer.h>
#include<SDL2/SDL_messagebox.h>

#include"ThreadManager.hpp"

ThreadManager* ThreadManager::Singleton = new ThreadManager();

void _workerTaskRunner(Worker* ThisWorker)
{
	while (true)
	{
		if (ThisWorker->TaskQueue.size() == 0)
		{
			ThisWorker->Status = WorkerStatus::WaitingForTask;

			SDL_Delay(50); // TODO could probably have a better method for giving the CPU time to do other stuff

			continue;
		}

		Task* CurrentTask = ThisWorker->TaskQueue[ThisWorker->TaskQueue.size() - 1];

		ThisWorker->Status = WorkerStatus::RunningTask;

		CurrentTask->Function(CurrentTask->FuncArgument);

		ThisWorker->TaskIdx++;

		delete CurrentTask;

		ThisWorker->TaskQueue.pop_back();
	}
}

void Worker::WaitForCurrentTaskFinish()
{
	uint32_t LastTask = this->TaskIdx;

	while ((LastTask == this->TaskIdx) || (this->Status != WorkerStatus::RunningTask))
		SDL_Delay(25);
}

void Worker::WaitForAllTasksFinish()
{
	while (this->Status != WorkerStatus::WaitingForTask)
		SDL_Delay(25);
}

ThreadManager::ThreadManager()
{
	this->ApplicationWindow = nullptr;
}

std::vector<Worker*> ThreadManager::GetWorkers()
{
	return this->m_Workers;
}

std::vector<Worker*> ThreadManager::CreateWorkers(int NumWorkers, WorkerType Type)
{
	std::vector<Worker*> NewWorkers;

	for (int WorkerIndex = 0; WorkerIndex < NumWorkers; WorkerIndex++)
	{
		Worker* NewWorker = new Worker();

		NewWorker->TaskQueue = std::vector<Task*>({});

		NewWorker->TaskQueue.resize(0);

		NewWorker->TaskIdx = 0;

		NewWorker->Type = WorkerType::DefaultTaskWorker;

		NewWorker->Thread = new std::thread(&_workerTaskRunner, NewWorker);
		NewWorker->Thread->detach();

		this->m_Workers.push_back(NewWorker);
		NewWorkers.push_back(NewWorker);
	}

	return NewWorkers;
}

void ThreadManager::DispatchJob(Task& Job)
{
	Worker* AssignedWorker = nullptr;

	for (int Index = 0; Index < this->m_Workers.size(); Index++)
	{
		Worker* CurrentWorker = this->m_Workers[Index];

		CurrentWorker->Type = WorkerType::DefaultTaskWorker;

		if (CurrentWorker->Type == WorkerType::DefaultTaskWorker)
		{
			if (CurrentWorker->Status == WorkerStatus::WaitingForTask)
				AssignedWorker = CurrentWorker;
			else
			{
				if (!AssignedWorker || (CurrentWorker->TaskQueue.size() < AssignedWorker->TaskQueue.size()))
					AssignedWorker = CurrentWorker;
			}
		}
	}

	if (!AssignedWorker)
	{
		//OnErrorEvent.Fire("Could not DispatchJob, number of workers may be too low or none are DefaultTaskWorker.");

		throw(std::string("Could not DispatchJob, there might not be any workers or none are a DefaultTaskWorker"));

		return;
	}

	// got a worker, tell it to do the task
	AssignedWorker->TaskQueue.push_back(&Job);
}

ThreadManager* ThreadManager::Get()
{
	if (!ThreadManager::Singleton)
	{
		throw(std::string("ThreadManager::Get was called before Engine was initialized."));
		return nullptr;
	}

	return ThreadManager::Singleton;
}
