// ComponentBase.hpp - templates and Component Manager virtual class, 31/01/2026
#pragma once

#include "datatype/EntityComponent.hpp"
#include "datatype/Ref.hpp"
#include "Reflection.hpp"

class GameObject;
struct BaseComponent;

class IComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) = 0;
    virtual std::vector<BaseComponent*> GetComponents() = 0;
    virtual void ForEachComponent(const std::function<bool(BaseComponent*)>) = 0;
    virtual BaseComponent* GetComponent(uint32_t) = 0;
    virtual void DeleteComponent(uint32_t ComponentId) = 0;
    virtual void BindService(uint32_t) = 0;
    virtual void UnbindService() = 0;
    virtual void Shutdown() = 0;
    virtual ~IComponentManager(); // just default, but moved to a .cpp file to anchor vtable

    virtual const Reflection::StaticPropertyMap& GetProperties() = 0;
    virtual const Reflection::StaticMethodMap& GetMethods() = 0;
    virtual const Reflection::StaticEventMap& GetEvents() = 0;
    virtual Reflection::GenericValue GetDefaultPropertyValue(const std::string_view&) = 0;
    virtual Reflection::GenericValue GetDefaultPropertyValue(const Reflection::PropertyDescriptor*) = 0;

    std::vector<Reflection::EventConnection> ComponentCreatedCallbacks;
    std::vector<Reflection::EventConnection> ComponentDeletedCallbacks;

    Reflection::EventDescriptor ComponentCreatedEvent = Reflection::EventDescriptor{
        .CallbackInputs = REFLECTION_SPAN({ Reflection::ValueType::GameObject }),
        .Connect = [this](void*, const Reflection::EventConnection& Callback)
            {
                return Reflection::EventConnect(ComponentCreatedCallbacks, Callback);
            },
        .Disconnect = [this](void*, uint32_t Id)
            {
                Reflection::EventDisconnect(ComponentCreatedCallbacks, Id);
            },
        .Cleanup = [this](void*)
            {
                Reflection::EventCleanup(ComponentCreatedCallbacks);
            }
    };
    Reflection::EventDescriptor ComponentDeletedEvent = Reflection::EventDescriptor{
        .CallbackInputs = REFLECTION_SPAN({ Reflection::ValueType::GameObject }),
        .Connect = [this](void*, const Reflection::EventConnection& Callback)
            {
                return Reflection::EventConnect(ComponentDeletedCallbacks, Callback);
            },
        .Disconnect = [this](void*, uint32_t Id)
            {
                Reflection::EventDisconnect(ComponentDeletedCallbacks, Id);
            },
        .Cleanup = [this](void*)
            {
                Reflection::EventCleanup(ComponentDeletedCallbacks);
            }
    };
};

void RegisterComponentManager(EntityComponent Type, IComponentManager*);
IComponentManager* GetComponentManagerByComponentType(EntityComponent Type);

template <class T>
class ComponentManager : public IComponentManager
{
public:
    static ComponentManager* Get()
    {
        return (ComponentManager*)GetComponentManagerByComponentType(T::Type);
    }

    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        uint32_t id = UINT32_MAX; //NextFreeId;
        ObjectRef ref = Object;

        if (id == UINT32_MAX)
        {
            id = static_cast<uint32_t>(Components.size());

            T& component = Components.emplace_back();
            component.Object = ref;
            component.Valid = true;
            assert(component.Valid);
        }
        else
        {
            T& component = Components[id];
            NextFreeId = component.NextFreeId;
            component = T();
        }

        Reflection::SignalRestrictedEvent(ref.TargetId, ComponentCreatedCallbacks, { Reflection::GenericValue(ref) }, "ComponentCreatedSignal");
        return id;
    }

    virtual BaseComponent* GetComponent(uint32_t Id) override
    {
        T& component = Components.at(Id);
        return component.Valid ? (BaseComponent*)&component : nullptr;
    }

    virtual std::vector<BaseComponent*> GetComponents() override
    {
        std::vector<BaseComponent*> v;
        v.reserve(Components.size());

        for (T& component : Components)
        {
            if (component.Valid)
                v.push_back((BaseComponent*)&component);
        }

        return v;
    }

    virtual void ForEachComponent(const std::function<bool(BaseComponent*)> Continue) override
    {
        for (T& component : Components)
            if (component.Valid && !Continue((BaseComponent*)&component))
                break;
    }

    virtual void DeleteComponent(uint32_t Id) override
    {
        T& component = Components.at(Id);
        for (auto& [ _, event ] : GetEvents())
            event.Cleanup((void*)&component);

        component.NextFreeId = NextFreeId;
        NextFreeId = Id;

        component.Valid = false;
        Reflection::SignalRestrictedEvent(component.Object.TargetId, ComponentDeletedCallbacks, { Reflection::GenericValue(component.Object) }, "ComponentDeletedSignal");
    }

    virtual void BindService(uint32_t) override
    {
    }

    virtual void UnbindService() override
    {
    }

    virtual void Shutdown() override
    {
        Components.clear();
    }

    virtual const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap properties;
        return properties;
    }

    virtual const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap methods;
        return methods;
    }

    virtual const Reflection::StaticEventMap& GetEvents() override
    {
        static const Reflection::StaticEventMap events;
        return events;
    }

    virtual Reflection::GenericValue GetDefaultPropertyValue(const std::string_view& Property) override
    {
        return GetDefaultPropertyValue(&GetProperties().at(Property));
    }

    virtual Reflection::GenericValue GetDefaultPropertyValue(const Reflection::PropertyDescriptor* Property) override
    {
        static T Defaults;
        return Property->Get((void*)&Defaults);
    }

    ComponentManager()
    {
        RegisterComponentManager(T::Type, this);
    }

    std::vector<T> Components;
    uint32_t NextFreeId = UINT32_MAX;
};

struct BaseComponent
{
    ObjectRef Object;
    uint32_t NextFreeId = UINT32_MAX;
};

template <EntityComponent T>
struct Component : BaseComponent
{
    static inline EntityComponent Type = T;
};
