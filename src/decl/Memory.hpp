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
	// if this list is changed, remember to update
	// `CategoryNames` further down
	enum class Category : uint8_t
	{
		Default = 0,
		GameObject,
		Reflection,
		RenderCommands,
		Mesh,
		Luau,

		__count
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

	inline std::array<size_t, static_cast<size_t>(Category::__count)> Counters{ 0 };

	static inline const char* CategoryNames[] =
	{
		"Default",
		"GameObject",
		"Reflection",
		"RenderCommands",
		"Mesh",
		"Luau"
	};

	static_assert(std::size(CategoryNames) == (uint8_t)Memory::Category::__count);

	// stl-conforming allocator
	template <class T>
	struct Allocator
	{
		T* allocate(size_t n);
		void deallocate(T* p, size_t n);

		T value_type;
	};
};

