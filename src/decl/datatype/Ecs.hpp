// Ecs.hpp, 15/10/2026
#pragma once

#include <unordered_map>
#include <functional>
#include <optional>
#include <cstdint>
#include <cassert>
#include <vector>
#include <span>

// See JECS.

using Id = uint64_t;
using Entity = Id;
using Component = Id;
using Pair = Id;

constexpr uint64_t EcsEntityMask = 1ull << 24;
constexpr uint64_t EcsPairOffset = 1ull << 48;

inline Pair pair(Id predicate, Id object)
{
    predicate %= EcsEntityMask;
    object %= EcsEntityMask;

    return object + (predicate * EcsEntityMask) + EcsPairOffset;
}

inline uint64_t pairFirst(Pair pair)
{
    return (pair - EcsPairOffset) / EcsEntityMask;
}

inline uint64_t pairSecond(Pair pair)
{
    return (pair - EcsPairOffset) % EcsEntityMask;
}

struct ComponentRecordBase
{
    std::optional<Component> getIndex(Entity e)
    {
        assert(e != 0);

        if (const auto& it = sparse.find(e); it != sparse.end())
        {
            if (dense[it->second] == e)
                return it->second;
        }

        return std::nullopt;
    }

    Component insert(Entity e)
    {
        assert(e != 0);

        if (std::optional<Component> c = getIndex(e))
            return c.value();

        uint64_t c = dense.size();
        dense.push_back(e);
        sparse.insert({ e, c });

        return c;
    }

    void remove(Entity e)
    {
        assert(e != 0);

        sparse.erase(e);
    }

    std::unordered_map<Entity, Component> sparse;
    std::vector<Entity> dense;
};

template <typename T>
struct ComponentRecord : public ComponentRecordBase
{
    std::vector<T> data;
};

struct LuauComponentRecord : ComponentRecordBase
{
    struct lua_State* owner = nullptr;
    std::vector<int> data;
};

struct World
{
    Entity entity()
    {
        return ++EntityCount;
    }

    Component component(ComponentRecordBase* cr)
    {
        uint64_t c = Components.size();
        Components.push_back(cr);

        return c;
    }

    void query(const std::span<Component>& predicates, const std::function<bool(Entity)>& keepGoing)
    {
        for (Entity e = 0; e < EntityCount; e++)
        {
            bool hasAll = true;

            for (Component c : predicates)
            {
                if (!Components[c]->getIndex(e))
                {
                    hasAll = false;
                    break;
                }
            }

            if (hasAll && !keepGoing(e))
                break;
        }
    }

    std::vector<ComponentRecordBase*> Components;
    uint64_t EntityCount = 0;
};
