// Ecs.cpp, 17/10/2026
#include <format>

#include "datatype/Ecs.hpp"

Id pair(Id predicate, Id object)
{
    predicate %= EcsEntityMask;
    object %= EcsEntityMask;

    return object + (predicate * EcsEntityMask) + EcsPairOffset;
}

Id pairFirst(Id pair)
{
    return (pair - EcsPairOffset) / EcsEntityMask;
}

Id pairSecond(Id pair)
{
    return (pair - EcsPairOffset) % EcsEntityMask;
}

bool isPair(Id id)
{
    return id > EcsPairOffset;
}

std::optional<Id> ComponentRecordBase::findIndex(Id e)
{
    if (const auto& it = sparse.find(e); it != sparse.end())
    {
        if (dense[it->second] == e)
            return it->second;
    }

    return std::nullopt;
}

Id ComponentRecordBase::insert(Id e)
{
    if (std::optional<Id> c = findIndex(e))
        return c.value();

    uint64_t c = dense.size();
    dense.push_back(e);
    sparse.insert({ e, c });
    insertData();

    return c;
}

void ComponentRecordBase::remove(Id e)
{
    sparse.erase(e);
}

Reflection::GenericValue PairRecord::getData(uint64_t index)
{
    // throw std::runtime_error("Pairs do not have any associated data");
    return Reflection::GenericValue::Null();
}

Reflection::GenericValue LuauComponentRecord::getData(uint64_t index)
{
    // This case is where we want to *serialize* the data instead of sharing it directly.
    throw std::runtime_error("Not implemented!");
}

Id World::entity()
{
    return ++EntityCount;
}

Id World::component(ComponentRecordBase* cr)
{
    Id c = ++ComponentCount;
    Predicates.insert({ c, cr });

    return c;
}

World::ComponentData World::add(Id e, Id c)
{
    if (isPair(c))
    {
        PairRecord* pr = new PairRecord;
        pr->insert(e);

        if (const auto& it = Predicates.find(c); it != Predicates.end())
            delete it->second;

        Predicates[c] = pr;
        return { .cr = pr, .index = 0 };
    }
    else
    {
        const auto& it = Predicates.find(c);
        if (it == Predicates.end())
            throw std::runtime_error(std::format("Cannot add invalid predicate '{}' to entity {} (expected component or pair ID)", c, e));

        return { .cr = it->second, .index = it->second->insert(e) };
    }
}

void World::remove(Id e, Id c)
{
    const auto& it = Predicates.find(c);
    if (it == Predicates.end())
        throw std::runtime_error(std::format("Cannot remove invalid predicate '{}' from entity {} (expected component or pair ID)", c, e));

    it->second->remove(e);
}

std::optional<World::ComponentData> World::find(Id e, Id c)
{
    if (const auto& it = Predicates.find(c); it != Predicates.end())
    {
        if (std::optional<uint64_t> index = it->second->findIndex(e))
            return ComponentData{ .cr = it->second, .index = index.value() };
    }

    return std::nullopt;
}

bool World::query(const std::span<Id>& predicates, const std::function<bool(Id, const std::vector<ComponentData>&)>& keepGoing, Id start)
{
    std::vector<ComponentRecordBase*> crs;
    crs.reserve(predicates.size());

    for (Id pred : predicates)
    {
        const auto& it = Predicates.find(pred);
        if (it == Predicates.end())
            throw std::runtime_error(std::format("Invalid query predicate '{}' (expected component or pair ID)", pred));

        crs.push_back(it->second);
    }

    std::vector<ComponentData> datas(predicates.size());

    Id e = start == UINT64_MAX ? 0 : start;

    for (; e < EntityCount; e++)
    {
        bool hasAll = true;

        for (size_t cri = 0; cri < crs.size(); cri++)
        {
            ComponentRecordBase* cr = crs[cri];

            if (std::optional<Id> c = cr->findIndex(e))
            {
                datas[cri] = ComponentData{ .cr = cr, .index = c.value() };
            }
            else
            {
                hasAll = false;
                break;
            }
        }

        if (hasAll && !keepGoing(e, datas))
            break;
    }

    return e >= EntityCount;
}

void World::shutdown()
{
    for (auto [ c, cr ] : Predicates)
    {
        if (isPair(c))
            delete cr;
    }

    Predicates.clear();
    ComponentCount = 0;
    EntityCount = 0;
}
