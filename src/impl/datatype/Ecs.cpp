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

Reflection::GenericValue LuauComponentRecord::getData(uint64_t index)
{
    // This case is where we want to *serialize* the data.
    throw std::runtime_error("getData on LuauComponentRecord: Not implemented!");
}

void LuauComponentRecord::setData(uint64_t index, const Reflection::GenericValue& value)
{
    // Ditto.
    throw std::runtime_error("setData on LuauComponentRecord: Not implemented!");
}

struct NameComponentRecord : public ComponentRecord<std::string>
{
    NameComponentRecord()
    {
        name = "Name";
    }

    Reflection::GenericValue getData(uint64_t index) override
    {
        return data[index];
    }

    void setData(uint64_t index, const Reflection::GenericValue& value) override
    {
        data[index] = value.AsString();
    }
};

struct TransformComponent
{
    glm::mat4 Local = { 1.f };
    glm::mat4 World = { 1.f };
};

struct TransformComponentRecord : public ComponentRecord<TransformComponent>
{
    TransformComponentRecord()
    {
        name = "Transform";

        fields = {
            { "Local", Reflection::FieldDescriptor{
                .Read = [](ComponentRecordBase* cr, uint64_t index) -> Reflection::GenericValue
                {
                    TransformComponentRecord* tcr = (TransformComponentRecord*)cr;
                    return tcr->data[index].Local;
                },
                .Write = [](ComponentRecordBase* cr, uint64_t index, const Reflection::GenericValue& value)
                {
                    TransformComponentRecord* tcr = (TransformComponentRecord*)cr;
                    tcr->data[index].Local = value.AsMatrix();
                },
                .Type = Reflection::ValueType::Matrix,
            } },
            { "World", Reflection::FieldDescriptor{
                .Read = [](ComponentRecordBase* cr, uint64_t index) -> Reflection::GenericValue
                {
                    TransformComponentRecord* tcr = (TransformComponentRecord*)cr;
                    return tcr->data[index].World;
                },
                .Write = [](ComponentRecordBase* cr, uint64_t index, const Reflection::GenericValue& value)
                {
                    TransformComponentRecord* tcr = (TransformComponentRecord*)cr;
                    tcr->data[index].World = value.AsMatrix();
                },
                .Type = Reflection::ValueType::Matrix,
                .Serializes = false,
            } },
        };
    }

    Reflection::GenericValue getData(uint64_t index) override
    {
        Reflection::GenericValue cell;
        cell.Val.Component = { .cr = this, .index = index };
        cell.Type = Reflection::ValueType::Component;

        return cell;
    }

    void setData(uint64_t index, const Reflection::GenericValue& value) override
    {
        value.ForEachMapPair([&](const Reflection::GenericValue& key, const Reflection::GenericValue& value)
        {
            std::string_view field = key.AsStringView();

            if (field == "Local")
                data[index].Local = value.AsMatrix();
            else if (field == "World")
                data[index].World = value.AsMatrix();
        });
    }

    const Reflection::FieldMap& getFields() override
    {
        return fields;
    }

    static inline Reflection::FieldMap fields;
};

void World::initialize()
{
    component(new NameComponentRecord);
    component(new TransformComponentRecord);
}

void World::shutdown()
{
    for (auto [ c, cr ] : predicates)
        delete cr;

    predicates.clear();
    componentCount = 0;
    entityCount = 0;
}

Id World::entity()
{
    return ++entityCount;
}

Id World::component(ComponentRecordBase* cr)
{
    Id c = ++componentCount;
    predicates.insert({ c, cr });

    return c;
}

World::ComponentData World::add(Id e, Id c)
{
    if (isPair(c))
    {
        const auto& it = predicates.find(pairFirst(c));
        if (it == predicates.end())
            throw std::runtime_error(std::format("Invalid relationship '{}' in pair", pairFirst(c)));

        if (!it->second->isLuau)
            throw std::runtime_error("Expected pair relation to be a Luau record");

        LuauComponentRecord* cr = nullptr;

        if (const auto& old = predicates.find(c); old != predicates.end())
        {
            if (!old->second->isLuau)
                throw std::runtime_error("Internal error: Pair was not a Luau record");

            cr = (LuauComponentRecord*)old->second;
        }
        else
        {
            cr = new LuauComponentRecord;
        }

        cr->insert(e);

        predicates[c] = cr;
        return { .cr = cr, .index = 0 };
    }
    else
    {
        const auto& it = predicates.find(c);
        if (it == predicates.end())
            throw std::runtime_error(std::format("Cannot add invalid predicate '{}' to entity {} (expected component or pair ID)", c, e));

        return { .cr = it->second, .index = it->second->insert(e) };
    }
}

void World::remove(Id e, Id c)
{
    const auto& it = predicates.find(c);
    if (it == predicates.end())
        throw std::runtime_error(std::format("Cannot remove invalid predicate '{}' from entity {} (expected component or pair ID)", c, e));

    it->second->remove(e);
}

std::optional<World::ComponentData> World::find(Id e, Id c)
{
    if (const auto& it = predicates.find(c); it != predicates.end())
    {
        if (std::optional<uint64_t> index = it->second->findIndex(e))
            return ComponentData{ .cr = it->second, .index = index.value() };
    }

    return std::nullopt;
}

bool World::query(const std::span<Id>& queryPredicates, const std::function<bool(Id, const std::vector<ComponentData>&)>& keepGoing, Id start)
{
    std::vector<ComponentRecordBase*> crs;
    crs.reserve(queryPredicates.size());

    for (Id pred : queryPredicates)
    {
        const auto& it = predicates.find(pred);
        if (it == predicates.end())
            throw std::runtime_error(std::format("Invalid query predicate '{}' (expected component or pair ID)", pred));

        crs.push_back(it->second);
    }

    std::vector<ComponentData> datas(queryPredicates.size());
    Id e = start;

    for (; e <= entityCount; e++)
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

    return e > entityCount;
}
