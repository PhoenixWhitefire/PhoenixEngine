// Parallel SharedMutex, 28/05/2026
#pragma once

#include <atomic>
#include <mutex>

struct SharedMutex
{
    std::string Name;
    std::mutex Mutex;
    std::atomic_int ReferenceCount;
};

// refcount == 0 `SharedMutex`es and `SharedBuffer`s
void CollectParallelResourceGarbage();
