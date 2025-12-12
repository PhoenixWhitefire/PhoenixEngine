#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

#include "Memory.hpp"

namespace hx
{
    template <Memory::Category C>
    using string = std::basic_string<char, std::char_traits<char>, Memory::Allocator<char, C>>;

    template <class T, Memory::Category C>
    using vector = std::vector<T, Memory::Allocator<T, C>>;

    template <class K, class V, Memory::Category C>
    using unordered_map = std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, std::allocator<std::pair<const K, V>>>;

    template <class T, Memory::Category C>
    using unordered_set = std::unordered_set<T, std::hash<T>, std::equal_to<T>, std::allocator<T>>;
}
