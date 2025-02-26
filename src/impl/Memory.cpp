// 24/01/2025
// this thing functions based on padding the allocated memory with an
// `AllocHeader` that describes the size and category of the allocation
// https://stackoverflow.com/a/1208728/16875161
// you can call it ugly all you want, but I really wanted to reduce cruft
// with these, instead of having to pass the Size and Category to the
// de-allocator as well
// not sure how Tracy does it. i feel like it has to be a hash table,
// but that sounds slow and even uglier

#include <tracy/Tracy.hpp>
#include <assert.h>
// 05/02/2025 HUH
// why is G++ like this
// builds in Debug, but not Release
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "Memory.hpp"

struct AllocHeader
{
	uint32_t Size = UINT32_MAX;
	uint8_t Category = UINT8_MAX;
	uint8_t Check = 222; // you probably do not know what this in reference to
};

namespace Memory
{
	void* GetPointerInfo(void* Pointer, size_t* Size, uint8_t* Category)
	{
		void* realPointer = (uint8_t*)Pointer - sizeof(AllocHeader);

		AllocHeader* header = (AllocHeader*)realPointer;
		// in case someone passes in a pointer that wasn't alloc'd by `::Alloc` and doesn't immediately segfault,
		// hold their hand and tell them they're
		assert(header->Check == 222);

		if (Size)
			*Size = header->Size;
		if (Category)
			*Category = header->Category;

		return realPointer;
	}

	void* Alloc(size_t Size, Memory::Category MemCat)
	{
		Size += sizeof(AllocHeader);

		assert(Size < UINT32_MAX);

		void* ptr = malloc(Size);

		if (ptr)
		{
			uint8_t memIndex = static_cast<uint8_t>(MemCat);

#if TRACY_ENABLE
			if (MemCat != Memory::Category::Default)
				TracyAllocN(ptr, Size, CategoryNames[memIndex]);
			else
				TracyAlloc(ptr, Size);
#endif

			Counters[memIndex] += Size;

			AllocHeader header{ static_cast<uint32_t>(Size), memIndex };
			memcpy(ptr, &header, sizeof(AllocHeader));

			return (void*)((uint8_t*)ptr + sizeof(AllocHeader));
		}
		else
			return NULL;
	}

	void* ReAlloc(void* Pointer, size_t Size, Memory::Category MemCat)
	{
		if (Pointer == nullptr)
			return Alloc(Size, MemCat);

		Size += sizeof(AllocHeader);

		assert(Size < UINT32_MAX);

		size_t prevSize = UINT64_MAX;
		Pointer = GetPointerInfo(Pointer, &prevSize);

		void* ptr = realloc(Pointer, Size);

		if (ptr)
		{
			uint8_t memIndex = static_cast<uint8_t>(MemCat);

#if TRACY_ENABLE
			if (MemCat != Memory::Category::Default)
			{
				TracyFreeN(Pointer, CategoryNames[memIndex]);
				TracyAllocN(ptr, Size, CategoryNames[memIndex]);
			}
			else
			{
				TracyFree(Pointer);
				TracyAlloc(ptr, Size);
			}
#endif
			Counters[memIndex] -= prevSize;
			Counters[memIndex] += Size;
			
			*((AllocHeader*)ptr) = AllocHeader{ static_cast<uint32_t>(Size), memIndex };

			return (void*)((uint8_t*)ptr + sizeof(AllocHeader));
		}
		else
			return NULL;
	}

	void Free(void* Pointer)
	{
		// default `free` does nothing if passed `NULL`
		if (!Pointer)
			return;

		uint64_t size = UINT64_MAX;
		uint8_t memcat = UINT8_MAX;

		Pointer = GetPointerInfo(Pointer, &size, &memcat);

		assert(memcat < static_cast<uint8_t>(Memory::Category::__count));

#if TRACY_ENABLE
		if (memcat != static_cast<uint8_t>(Memory::Category::Default))
			TracyFreeN(Pointer, CategoryNames[memcat]);
		else
			TracyFree(Pointer);
#endif

		assert(Counters[memcat] > 0);
		Counters[memcat] -= size;

		free(Pointer);
	}
};

template <class T>
T* Memory::Allocator<T>::allocate(size_t n)
{
	return (T*)Alloc(n, Category::Default);
}

template <class T>
void Memory::Allocator<T>::deallocate(T* p, size_t n)
{
	Free(p);
}
