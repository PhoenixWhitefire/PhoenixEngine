#include "ThreadManager.hpp"

static void _workerTaskRunner(Worker* ThisWorker)
{
	while (true)
	{
		if (ThisWorker->TaskQueue.empty())
		{
			ThisWorker->Status = WorkerStatus::WaitingForTask;

			std::this_thread::sleep_for(std::chrono::milliseconds(5));

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
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

void Worker::WaitForAllTasksFinish() const
{
	while (this->Status != WorkerStatus::WaitingForTask)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
	static ThreadManager inst;
	return &inst;
}
