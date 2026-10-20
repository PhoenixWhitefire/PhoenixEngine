// Ecs.cpp, 17/10/2026
#include <format>

#include "datatype/Ecs.hpp"
#include "datatype/BuiltinComponents.hpp"

Id pair(Id relation, Id object)
{
    relation %= EcsEntityMask;
    object %= EcsEntityMask;

    return object + (relation * EcsEntityMask) + EcsPairOffset;
}

Id pairFirst(Id p)
{
    assert(isPair(p));
    return (p - EcsPairOffset) / EcsEntityMask;
}

Id pairSecond(Id p)
{
    assert(isPair(p));
    return (p - EcsPairOffset) % EcsEntityMask;
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
        value.ForEachMapPair([&](const Reflection::GenericValue& k, const Reflection::GenericValue& v)
        {
            std::string_view field = k.AsStringView();

            if (field == "Local")
                data[index].Local = v.AsMatrix();
            else if (field == "World")
                data[index].World = v.AsMatrix();
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
    assert(predicates.size() == 0 && "World::initialize called multiple times!");

    NameComponentRecord* ncr = new NameComponentRecord;

    Id wildcard = component(new LuauComponentRecord);
    Id name = component(ncr);
    Id childOf = component(new LuauComponentRecord);
    Id transform = component(new TransformComponentRecord);

    assert(wildcard == Ec::Wildcard);
    assert(name == Ec::Name);
    assert(childOf == Ec::ChildOf);
    assert(transform == Ec::Transform);

    (void)wildcard;
    (void)name;
    (void)childOf;
    (void)transform;

    for (Id c = Ec::start; c <= Ec::count; c++)
    {
        ComponentData cName = add(c, name);
        ncr->data[cName.index] = EcNames[c - 1];
    }
}

void World::shutdown()
{
    for (auto [ c, cr ] : predicates)
        delete cr;

    predicates.clear();
    entityCount = 0;
}

Id World::entity()
{
    return ++entityCount;
}

Id World::component(ComponentRecordBase* cr)
{
    Id c = ++entityCount;
    predicates.insert({ c, cr });

    return c;
}

static ComponentRecordBase* addPair(World* world, Id e, Id p)
{
    LuauComponentRecord* cr = nullptr;

    if (const auto& old = world->predicates.find(p); old != world->predicates.end())
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
    world->predicates[p] = cr;

    return cr;
}

World::ComponentData World::add(Id e, Id c)
{
    if (isPair(c))
    {
        const auto& it = predicates.find(pairFirst(c));
        if (it == predicates.end())
            throw std::runtime_error(std::format("Invalid relationship '{}' in pair", pairFirst(c)));

        addPair(this, e, pair(pairFirst(c), Ec::Wildcard));
        addPair(this, e, pair(Ec::Wildcard, pairSecond(c)));

        return { .cr = addPair(this, e, c), .index = 0 };
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
        {
            if (isPair(pred))
                return true; // Relationship not used anywhere.
            else
                throw std::runtime_error(std::format("Internal error: Invalid component {:x}", pred)); // It should be impossible to get a non-pair ID that doesn't refer to anything.
        }

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
