// 21/01/2025
// i dont think this is worth doing for the engine at it's current stage
// going to work on animations now

#pragma once

#include <stdint.h>
#include <array>

#define MEM_ALLOC_OPERATORS(struc, cat) static void* operator new(size_t Count) \
{ \
	return Memory::Alloc(sizeof(struc) * Count, Memory::Category::cat); \
} \
static void* operator new[](size_t Count) \
{ \
	return Memory::Alloc(sizeof(struc) * Count, Memory::Category::cat); \
} \
void operator delete(void* Ptr) \
{ \
	Memory::Free(Ptr); \
} \

namespace Memory
{
	enum class Category : uint8_t
	{
		Default = 0,
		GameObject,
		Reflection,
		Rendering,
		Mesh,
		Luau,

		_count
	};

	// can only be de-alloc'd correctly by `::Free`
	// ONLY UP TO `UINT32_MAX` IN SIZE IN REALITY!!
	void* Alloc(size_t, Category MemCat = Category::Default);
	// can only be de-alloc'd correctly by `::Free`
	// ONLY UP TO `UINT32_MAX` IN SIZE IN REALITY!!
	void* ReAlloc(void*, size_t, Category MemCat = Category::Default);
	// can only safely de-alloc pointers returned by `::Alloc`
	void Free(void*);
	// returns the pointer to the actual `malloc` block
	void* GetPointerInfo(void*, size_t* Size = nullptr, uint8_t* Category = nullptr);

	inline std::array<size_t, static_cast<size_t>(Category::_count)> Counters{ 0 };
};
