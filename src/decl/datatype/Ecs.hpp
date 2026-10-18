// Ecs.hpp, 15/10/2026
#pragma once

#include <unordered_map>
#include <functional>
#include <optional>
#include <cstdint>
#include <cassert>
#include <vector>
#include <span>

#include "Reflection.hpp"

// See JECS.

using Id = uint64_t;

constexpr uint64_t EcsEntityMask = 1ull << 24;
constexpr uint64_t EcsPairOffset = 1ull << 48;

Id pair(Id predicate, Id object);
Id pairFirst(Id pair);
Id pairSecond(Id pair);
bool isPair(Id c);

struct ComponentRecordBase
{
    virtual ~ComponentRecordBase() = default;

    std::optional<Id> findIndex(Id e);

    virtual void insertData() = 0;

    Id insert(Id e);
    void remove(Id e);

    virtual Reflection::GenericValue getData(uint64_t index) = 0;
    virtual void setData(uint64_t index, const Reflection::GenericValue& value) = 0;

    virtual const Reflection::FieldMap& getFields()
    {
        throw std::runtime_error("No reflection data");
    }

    std::string name;

    std::unordered_map<Id, Id> sparse;
    std::vector<Id> dense;
    bool isLuau = false;
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

struct LuauComponentRecord : ComponentRecordBase
{
    LuauComponentRecord()
    {
        isLuau = true;
    }

    void insertData() override
    {
        data.push_back(0);
    }

    Reflection::GenericValue getData(uint64_t index) override;
    void setData(uint64_t index, const Reflection::GenericValue& value) override;

    struct lua_State* owner = nullptr;
    std::vector<int> data;
};

struct World
{
    void initialize();
    void shutdown();

    Id entity();
    Id component(ComponentRecordBase* cr);

    struct ComponentData
    {
        ComponentRecordBase* cr = nullptr;
        uint64_t index = UINT64_MAX;
    };

    ComponentData add(Id e, Id c);
    void remove(Id e, Id c);
    std::optional<ComponentData> find(Id e, Id c);
    bool query(
        const std::span<Id>& predicates,
        const std::function<bool(Id, const std::vector<ComponentData>&)>& keepGoing,
        Id start = 0
    );

    std::unordered_map<Id, ComponentRecordBase*> predicates;
    uint64_t componentCount = 0;
    uint64_t entityCount = 0;
};
