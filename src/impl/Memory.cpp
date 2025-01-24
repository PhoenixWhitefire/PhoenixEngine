#include <tracy/Tracy.hpp>
#include <assert.h>

#include "Memory.hpp"

#undef malloc
#undef free

struct AllocHeader
{
	uint32_t Size = UINT32_MAX;
	uint8_t Category = UINT8_MAX;
	uint16_t Check = 269420ui16;
};

static const char* MemCatNames[] =
{
	"Default",
	"GameObject",
	"Reflection",
	"Rendering",
	"Texture",
	"Mesh"
};

static_assert(std::size(MemCatNames) == (uint8_t)Memory::Category::_count);

namespace Memory
{
	void* GetPointerInfo(void* Pointer, size_t* Size, uint8_t* Category)
	{
		void* realPointer = (uint8_t*)Pointer - sizeof(AllocHeader);

		AllocHeader* header = (AllocHeader*)realPointer;

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
				TracyAllocN(ptr, Size, MemCatNames[memIndex]);
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
				TracyFreeN(Pointer, MemCatNames[memIndex]);
				TracyAllocN(ptr, Size, MemCatNames[memIndex]);
			}
			else
			{
				TracyFree(Pointer);
				TracyAlloc(ptr, Size);
			}
#endif
			Counters[memIndex] -= prevSize;
			Counters[memIndex] += Size;
			
			AllocHeader header{ static_cast<uint32_t>(Size), memIndex };
			memcpy(ptr, &header, sizeof(AllocHeader));

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

		assert(memcat < static_cast<uint8_t>(Memory::Category::_count));

#if TRACY_ENABLE
		if (memcat != static_cast<uint8_t>(Memory::Category::Default))
			TracyFreeN(Pointer, MemCatNames[memcat]);
		else
			TracyFree(Pointer);
#endif

		assert(Counters[memcat] > 0);
		Counters[memcat] -= size;

		free(Pointer);
	}
};
