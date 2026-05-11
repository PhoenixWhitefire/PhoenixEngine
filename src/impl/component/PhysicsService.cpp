// Physics Service, 23/01/2026
#include "component/PhysicsService.hpp"
#include "datatype/GameObject.hpp"

#define PROPERTY_PROXY(n, t) REFLECTION_PROPERTY( \
    #n, \
    t, \
    [](void* p) -> Reflection::GenericValue \
    { \
        EcPhysicsService* ep = static_cast<EcPhysicsService*>(p); \
        return ep->n; \
    }, \
    [](void* p, const Reflection::GenericValue& gv) \
    { \
        EcPhysicsService* ep = static_cast<EcPhysicsService*>(p); \
        ep->n = gv.As##t(); \
        if (ep->IsServiceInstance) \
            Physics::Get()->n = gv.As##t(); \
    } \
)

const Reflection::StaticPropertyMap& PhysicsComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        PROPERTY_PROXY(Timescale, Double),
        PROPERTY_PROXY(Simulating, Boolean),
        PROPERTY_PROXY(Gravity, Vector3),
        PROPERTY_PROXY(DebugCollisionAabbs, Boolean),
        PROPERTY_PROXY(DebugContactPoints, Boolean),
        PROPERTY_PROXY(DebugSpatialHeat, Boolean),
    };

    return props;
};

const Reflection::StaticMethodMap& PhysicsComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap& methods = {
        { "SetSimulationForcePaused", Reflection::MethodDescriptor{
            { Reflection::ValueType::Boolean },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                Physics::Get()->SimulatingForcePaused = inputs[0].AsBoolean();
                return {};
            }
        } }
    };

    return methods;
}

uint32_t PhysicsComponentManager::CreateComponent(GameObject* Object)
{
    uint32_t id = ComponentManager<EcPhysicsService>::CreateComponent(Object);
    m_Components[id].Object = Object;

    return id;
}

static EcPhysicsService DefaultInstance;

static void applyProperties(EcPhysicsService* ep, PhysicsComponentManager* pcm)
{
    for (const auto& [ _, prop ] : pcm->GetProperties())
    {
        if (prop.Set)
            prop.Set(ep, prop.Get(ep));
    }
}

void PhysicsComponentManager::BindService(uint32_t Id)
{
    EcPhysicsService* ep = (EcPhysicsService*)GetComponent(Id);

    ServiceInstance = ep->Object;
    DefaultInstance.IsServiceInstance = false;
    ep->IsServiceInstance = true;

    applyProperties(ep, this);
}

void PhysicsComponentManager::UnbindService()
{
    GetService()->IsServiceInstance = false;
    ServiceInstance.Clear();
    DefaultInstance.IsServiceInstance = true;

    applyProperties(&DefaultInstance, this);
}

EcPhysicsService* PhysicsComponentManager::GetService() const
{
    if (ServiceInstance && ServiceInstance->FindComponent<EcPhysicsService>())
        return ServiceInstance->FindComponent<EcPhysicsService>();
    else
        return &DefaultInstance;
}
