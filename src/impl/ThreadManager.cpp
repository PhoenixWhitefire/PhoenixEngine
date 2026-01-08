#include <tracy/Tracy.hpp>
#include <format>
#include <assert.h>

#include "ThreadManager.hpp"
#include "Log.hpp"

// TODO: Not sure why this happens 
#ifndef TracyFiberEnter
#define TracyFiberEnter(name) (void)name
#endid

#ifndef TracyFiberLeave
#define TracyFiberLeave (void)
#endif

// https://stackoverflow.com/a/23899379
#ifdef _WIN32
#include <windows.h>
const DWORD MS_VC_EXCEPTION=0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // Must be 0x1000.
   LPCSTR szName; // Pointer to name (in user addr space).
   DWORD dwThreadID; // Thread ID (-1=caller thread).
   DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)


static void SetThreadName(uint32_t dwThreadID, const char* threadName)
{

  // DWORD dwThreadID = ::GetThreadId( static_cast<HANDLE>( t.native_handle() ) );

   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = threadName;
   info.dwThreadID = dwThreadID;
   info.dwFlags = 0;

   __try
   {
      RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
   }
   __except(EXCEPTION_EXECUTE_HANDLER)
   {
   }
}
static void SetThreadName( const char* threadName)
{
    SetThreadName(GetCurrentThreadId(),threadName);
}

#undef min
#undef max

#elif defined(__linux__)
#include <sys/prctl.h>
static void SetThreadName( const char* threadName)
{
  prctl(PR_SET_NAME,threadName,0,0,0);
}

#else
static void SetThreadName(std::thread* thread, const char* threadName)
{
   auto handle = thread->native_handle();
   pthread_setname_np(handle,threadName);
}
#endif

static ThreadManager* s_Instance;

const char* s_WorkerFiberNames[] = 
{
	"Phx_Worker1",
	"Phx_Worker2",
	"Phx_Worker3",
	"Phx_Worker4",
	"Phx_Worker5",
	"Phx_Worker6",
	"Phx_Worker7",
	"Phx_Worker8"
};

// 09/05/2025
// https://www.geeksforgeeks.org/thread-pool-in-cpp/
void ThreadManager::Initialize(int NumThreadsOverride)
{
	ZoneScoped;

	size_t numThreads = static_cast<size_t>(std::max(std::thread::hardware_concurrency() * 0.75f, 3.f));
	numThreads = std::min(numThreads, static_cast<size_t>(8ull));

	if (NumThreadsOverride > -1)
		numThreads = static_cast<size_t>(NumThreadsOverride);

	Log::InfoF("Creating {} parallel threads...", numThreads);

	for (size_t i = 0; i < numThreads; i++)
	{
		m_Workers.emplace_back(
			[this, i]
			{
				TracyFiberEnter(s_WorkerFiberNames[i]);
				SetThreadName(s_WorkerFiberNames[i]);

				while (true)
				{
					Task task;

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

						if (m_Stop && !task.IsCritical)
							continue;
					}

					ZoneScopedN("Task");

					task.Function();
				}

				TracyFiberLeave;
			}
		);
	}

	s_Instance = this;

	Log::Info("ThreadManager initialized");
}

void ThreadManager::Dispatch(std::function<void()> Task, bool IsCritical)
{
	assert(s_Instance == this);
	assert(!m_Stop);

	if (m_Workers.size() == 0)
	{
		Task();
		return;
	}

	{
		std::unique_lock<std::mutex> lock{ m_TasksMutex };
		m_Tasks.emplace(std::move(Task), IsCritical);
	}

	m_TasksCv.notify_one();
}

void ThreadManager::m_StopThreads()
{
	ZoneScoped;

	{
		std::unique_lock<std::mutex> lock{ m_TasksMutex };
		m_Stop = true;
	}

	m_TasksCv.notify_all();

	for (std::thread& worker : m_Workers)
		if (worker.joinable())
			worker.join();
}

void ThreadManager::Shutdown()
{
	ZoneScoped;

	m_StopThreads();

	s_Instance = nullptr;
}

ThreadManager::~ThreadManager()
{
	ZoneScoped;

	// we crashed before `::Shutdown` was called
	if (s_Instance)
	{
		m_StopThreads();
		s_Instance = nullptr;
	}
}

ThreadManager* ThreadManager::Get()
{
	assert(s_Instance);
	return s_Instance;
}
