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

	void FrameFinish();

	inline std::array<size_t, static_cast<size_t>(Category::__count)> Counters{ 0 };
	inline std::array<size_t, static_cast<size_t>(Category::__count)> Activity{ 0 };

	static inline const char* CategoryNames[] =
	{
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
		"Sound"
	};

	static_assert(std::size(CategoryNames) == (uint8_t)Memory::Category::__count);

	// stl-conforming allocator
	template <class T, Category C = Category::Default>
	struct Allocator
	{
		typedef T value_type;

		T* allocate(size_t n)
		{
			return (T*)Alloc(n, C);
		}
		void deallocate(T* p, size_t n)
		{
			Free((void*)p);
		}
	};
	
	template <class T, Category C = Category::Default>
	using vector = std::vector<T>;
	//using vector = std::vector<T, Allocator<T, C>>;

	template <class T, Category C = Category::Default>
	using unordered_set = std::unordered_set<T>;
	//using unordered_set = std::unordered_set<T, std::hash<T>, std::equal_to<T>, Allocator<T, C>>;

	template <class K, class V, Category C = Category::Default>
	using unordered_map = std::unordered_map<K, V>;
	//using unordered_map = std::unordered_map<K, V, std::hash<T>, std::equal_to<T>, Allocator<T, C>>;

	template <Category C = Category::Default>
	using string = std::string;
	//using string = std::basic_string<char, std::char_traits<char>, Allocator<char, C>>;
};
