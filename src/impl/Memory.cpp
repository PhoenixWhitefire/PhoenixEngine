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

static std::array<size_t, static_cast<size_t>(Memory::Category::__count)> s_ActivityWip{ 0 };

void* Memory::GetPointerInfo(void* Pointer, uint32_t* Size, uint8_t* Category)
{
	AllocHeader* header = (AllocHeader*)((uintptr_t)Pointer - sizeof(AllocHeader));
	void* realPointer = (void*)((uintptr_t)header);

	// in case someone passes in a pointer that wasn't alloc'd by `::Alloc` and doesn't immediately segfault,
	// hold their hand and tell them they're
	assert(header->Check == 222);

	if (Size)
		*Size = header->Size;
	if (Category)
		*Category = header->Category;

	return realPointer;
}

void* Memory::Alloc(uint32_t Size, Memory::Category MemCat)
{
	assert(Size != 0);

	Size += sizeof(AllocHeader);
	void* ptr = malloc(Size * 2);

	if (ptr)
	{
		uint8_t memIndex = static_cast<uint8_t>(MemCat);

#ifdef TRACY_ENABLE
		if (MemCat != Memory::Category::Default)
			TracyAllocN(ptr, Size, CategoryNames[memIndex]);
		else
			TracyAlloc(ptr, Size);
#endif

		Counters[memIndex] += Size;
		s_ActivityWip[memIndex] += Size;

		AllocHeader header = { .Size = Size, .Category = memIndex };
		memcpy((void*)((uintptr_t)ptr), &header, sizeof(AllocHeader));

		return (void*)((uintptr_t)ptr + sizeof(AllocHeader));
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

void* Memory::ReAlloc(void* Pointer, uint32_t Size, Memory::Category MemCat)
{
	if (Pointer == nullptr)
		return Alloc(Size, MemCat);

	Size += sizeof(AllocHeader);

	uint32_t prevSize = UINT32_MAX;
	Pointer = GetPointerInfo(Pointer, &prevSize);

	// stupid g++ with stupid "may be used after `void* realloc"
	// SHUT UP
	// I KNOW WHAT I'M DOING
	// TODO HOW TO FIX THAT
	// 18/05/2025
	uint8_t memIndex = static_cast<uint8_t>(MemCat);
		
#ifdef TRACY_ENABLE
	if (MemCat != Category::Default)
		TracyFreeN(Pointer, CategoryNames[memIndex]);
	else
		TracyFree(Pointer);
#endif

	void* ptr = realloc(Pointer, Size * 2);

	if (ptr)
	{
#ifdef TRACY_ENABLE
		if (MemCat != Memory::Category::Default)
			TracyAllocN(ptr, Size, CategoryNames[memIndex]);
		else
			TracyAlloc(ptr, Size);
#endif
		Counters[memIndex] -= prevSize;
		Counters[memIndex] += Size;
		s_ActivityWip[memIndex] += prevSize + Size;

		AllocHeader header = { .Size = Size, .Category = memIndex };
		memcpy((void*)((uintptr_t)ptr), &header, sizeof(AllocHeader));

		return (void*)((uintptr_t)ptr + sizeof(AllocHeader));
	}
	else
		return nullptr;
}

void Memory::Free(void* Pointer)
{
	// default `free` does nothing if passed `NULL`
	if (!Pointer)
		return;

	uint32_t size = UINT32_MAX;
	uint8_t memcat = UINT8_MAX;

	Pointer = GetPointerInfo(Pointer, &size, &memcat);

	assert(memcat < static_cast<uint8_t>(Memory::Category::__count));

#ifdef TRACY_ENABLE
	if (memcat != static_cast<uint8_t>(Memory::Category::Default))
		TracyFreeN(Pointer, CategoryNames[memcat]);
	else
		TracyFree(Pointer);
#endif

	assert(Counters[memcat] > 0);
	Counters[memcat] -= size;
	s_ActivityWip[memcat] += size;

	free(Pointer);
}

void Memory::FrameFinish()
{
	Activity = s_ActivityWip;
	s_ActivityWip.fill(0.f);
}

// TODO: causes crash in GLFW init: _glfwInitEgl -> eglQueryString -> ...
/*
void* operator new(size_t size)
{
	assert(size < (size_t)UINT32_MAX);

	void* ptr = Memory::Alloc((uint32_t)size);
	if (!ptr)
		throw std::bad_alloc();

	return ptr;
}

void operator delete(void* ptr) noexcept
{
	Memory::Free(ptr);
}

void operator delete(void* ptr, size_t) noexcept
{
	Memory::Free(ptr);
}
*/
