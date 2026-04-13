// Physics Service, 23/01/2026
#include "component/PhysicsService.hpp"

#define PROPERTY_PROXY(n, t) REFLECTION_PROPERTY( \
    #n, \
    t, \
    [](void*) -> Reflection::GenericValue \
    { \
        return Physics::Get()->n; \
    }, \
    [](void*, const Reflection::GenericValue& gv) \
    { \
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
