// 21/01/2025
// i dont think this is worth doing for the engine at it's current stage
// going to work on animations now

#pragma once

#include <stdint.h>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <array>

#define MEMCAT(cat) Memory::Category::cat

namespace Memory
{
	// if this list is changed, remember to update
	// `CategoryNames` further down
	enum class Category : uint8_t
	{
		Default = 0,
		GameObject,
		Reflection,
		Rendering,
		Mesh,
		Texture,
		Shader,
		Material,
		Physics,
		Luau,
		Sound,
		Glfw,

		__count
	};

	// can only be de-alloc'd correctly by `::Free`
	void* Alloc(uint32_t Size, Category MemCat = Category::Default);
	// can only be de-alloc'd correctly by `::Free`
	void* ReAlloc(void*, uint32_t Size, Category MemCat = Category::Default);
	// can only safely de-alloc pointers returned by `::Alloc`
	void Free(void*);
	// returns the pointer to the actual `malloc` block
	void* GetPointerInfo(void*, uint32_t* Size = nullptr, uint8_t* Category = nullptr);

	void FrameFinish();

	inline std::array<size_t, static_cast<size_t>(Category::__count)> Counters;
	inline std::array<size_t, static_cast<size_t>(Category::__count)> Activity;

	static inline const char* CategoryNames[] = {
		"Default",
		"GameObject",
		"Reflection",
		"Rendering",
		"Mesh",
		"Texture",
		"Shader",
		"Material",
		"Physics",
		"Luau",
		"Sound",
		"Glfw"
	};

	static_assert(std::size(CategoryNames) == (uint8_t)Memory::Category::__count);

	// stl-conforming allocator
	template <class T, Category C = Category::Default>
	struct Allocator
	{
		using value_type = T;

		Allocator() noexcept = default;

		T* allocate(size_t n)
		{
			size_t bytes = sizeof(T) * n;
			assert(bytes < (size_t)UINT32_MAX);
			return (T*)Alloc((uint32_t)bytes, C);
		}
		void deallocate(T* p, size_t)
		{
			Free((void*)p);
		}

		template <class U>
		struct rebind
		{
			using other = Allocator<U, C>;
		};
	};
};

template <class T, class U, Memory::Category C>
bool operator ==(const Memory::Allocator<T, C>&, const Memory::Allocator<U, C>&) noexcept
{
	return true;
}

template <class T, class U, Memory::Category C>
bool operator !=(const Memory::Allocator<T, C>&, const Memory::Allocator<U, C>&) noexcept
{
	return false;
}
