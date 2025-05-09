#include <tracy/public/tracy/Tracy.hpp>
#include <format>

#include "ThreadManager.hpp"
#include "Log.hpp"

static ThreadManager* s_Instance;

const char* s_WorkerFiberNames[] = 
{
	"Worker1",
	"Worker2",
	"Worker3",
	"Worker4",
	"Worker5",
	"Worker6",
	"Worker7",
	"Worker8"
};

// 09/05/2025
// https://www.geeksforgeeks.org/thread-pool-in-cpp/
void ThreadManager::Initialize(int NumThreadsOverride)
{
	ZoneScoped;

	size_t numThreads = static_cast<size_t>(std::max(std::thread::hardware_concurrency() * 0.75f, 3.f));
	numThreads = std::min(numThreads, static_cast<size_t>(8ull));

	if (NumThreadsOverride > 0)
		numThreads = static_cast<size_t>(NumThreadsOverride);

	Log::Info(std::vformat("Creating {} parallel threads...", std::make_format_args(numThreads)));

	for (size_t i = 0; i < numThreads; i++)
	{
		m_Workers.emplace_back(
			[this, i]
			{
				TracyFiberEnter(s_WorkerFiberNames[i]);

				while (true)
				{
					std::function<void()> task;

					{
						std::unique_lock<std::mutex> lock{ m_TasksMutex };

						m_TasksCv.wait(
							lock,
							[this]
							{
								return !m_Tasks.empty() || m_Stop;
							}
						);

						if (m_Stop && m_Tasks.empty())
						{
							TracyFiberLeave;
							return;
						}

						task = std::move(m_Tasks.front());
						m_Tasks.pop();
					}

					{
						ZoneScopedN("Task");
						task();
					}
				}

				TracyFiberLeave;
			}
		);
	}

	s_Instance = this;

	Log::Info("ThreadManager initialized");
}

void ThreadManager::Dispatch(std::function<void()> Task)
{
	assert(s_Instance);
	assert(!m_Stop);
	assert(s_Instance == this);

	{
		std::unique_lock<std::mutex> lock{ m_TasksMutex };
		m_Tasks.emplace(std::move(Task));
	}

	m_TasksCv.notify_one();
}

ThreadManager::~ThreadManager()
{
	ZoneScoped;

	{
		std::unique_lock<std::mutex> lock{ m_TasksMutex };
		m_Stop = true;
	}

	m_TasksCv.notify_all();

	for (std::thread& worker : m_Workers)
		worker.join();
}

ThreadManager* ThreadManager::Get()
{
	return s_Instance;
}
