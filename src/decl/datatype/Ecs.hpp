// Ecs.hpp, 15/10/2026
#pragma once

#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <optional>
#include <cstdint>
#include <cassert>
#include <vector>
#include <format>
#include <span>

// See JECS.

using Id = uint64_t;
using Entity = Id;
using Component = Id;
using Pair = Id;

constexpr uint64_t EcsEntityMask = 1ull << 24;
constexpr uint64_t EcsPairOffset = 1ull << 48;
constexpr uint64_t EcsComponentMax = 256;

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

inline bool isPair(Id c)
{
    return c > EcsPairOffset;
}

struct ComponentRecordBase
{
    virtual ~ComponentRecordBase();

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

    virtual void insertData() = 0;

    Component insert(Entity e)
    {
        assert(e != 0);

        if (std::optional<Component> c = getIndex(e))
            return c.value();

        uint64_t c = dense.size();
        dense.push_back(e);
        sparse.insert({ e, c });
        insertData();

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
    void insertData() override
    {
        data.emplace_back();
    }

    std::vector<T> data;
};

struct PairRecord : public ComponentRecordBase
{
    void insertData() override
    {
    }
};

struct LuauComponentRecord : ComponentRecordBase
{
    void insertData() override
    {
        data.push_back(0);
    }

    struct lua_State* owner = nullptr;
    std::vector<int> data;
};

struct World
{
    Entity entity()
    {
        return ++EntityCount + EcsComponentMax;
    }

    Component component(ComponentRecordBase* cr)
    {
        uint64_t c = Components.size();
        if (c >= EcsComponentMax)
            throw std::runtime_error("Too many components!");

        Components.insert({ c, cr });
        return c;
    }

    void add(Entity e, Component c)
    {
        if (isPair(c))
        {
            PairRecord* pr = new PairRecord;
            pr->insert(e);

            if (const auto& it = Components.find(c); it != Components.end())
                delete it->second;

            Components[c] = pr;
        }
        else
        {
            const auto& it = Components.find(c);
            if (it == Components.end())
                throw std::runtime_error(std::format("Cannot add invalid predicate '{}' to entity {} (expected component or pair ID)", c, e));

            it->second->insert(e);
        }
    }

    void remove(Entity e, Component c)
    {
        const auto& it = Components.find(c);
        if (it == Components.end())
            throw std::runtime_error(std::format("Cannot remove invalid predicate '{}' from entity {} (expected component or pair ID)", c, e));

        it->second->remove(e);
    }

    void query(const std::span<Id>& predicates, const std::function<bool(Entity)>& keepGoing)
    {
        std::vector<ComponentRecordBase*> crs;
        crs.reserve(predicates.size());

        for (Id pred : predicates)
        {
            const auto& it = Components.find(pred);
            if (it == Components.end())
                throw std::runtime_error(std::format("Invalid query predicate '{}' (expected component or pair ID)", pred));

            crs.push_back(it->second);
        }

        for (Entity e = 0; e < EntityCount; e++)
        {
            bool hasAll = true;

            for (ComponentRecordBase* cr : crs)
            {
                if (!cr->getIndex(e))
                {
                    hasAll = false;
                    break;
                }
            }

            if (hasAll && !keepGoing(e))
                break;
        }
    }

    void shutdown()
    {
        for (auto [ c, cr ] : Components)
        {
            if (isPair(c))
                delete cr;
        }

        Components.clear();
        EntityCount = 0;
    }

    std::unordered_map<Id, ComponentRecordBase*> Components;
    uint64_t EntityCount = 0;
};
