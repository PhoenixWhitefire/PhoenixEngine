// Physics Service, 23/01/2026
#include "component/PhysicsService.hpp"

class PhysicsServiceManager : public ComponentManager<EcPhysicsService>
{
public:
    virtual const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
            REFLECTION_PROPERTY(
                "Timescale",
                Double,
                [](void*) -> Reflection::GenericValue
                {
                    return Physics::Get()->Timescale;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Physics::Get()->Timescale = gv.AsDouble();
                }
            ),

            REFLECTION_PROPERTY(
                "Simulating",
                Boolean,
                [](void*) -> Reflection::GenericValue
                {
                    return Physics::Get()->Simulating;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Physics::Get()->Simulating = gv.AsBoolean();
                }
            ),

            REFLECTION_PROPERTY(
                "Gravity",
                Vector3,
                [](void*) -> Reflection::GenericValue
                {
                    return Physics::Get()->Gravity;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Physics::Get()->Gravity = gv.AsVector3();
                }
            ),

            REFLECTION_PROPERTY(
                "DebugCollisionAabbs",
                Boolean,
                [](void*) -> Reflection::GenericValue
                {
                    return Physics::Get()->DebugCollisionAabbs;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Physics::Get()->DebugCollisionAabbs = gv.AsBoolean();
                }
            ),

            REFLECTION_PROPERTY(
                "DebugContactPoints",
                Boolean,
                [](void*) -> Reflection::GenericValue
                {
                    return Physics::Get()->DebugContactPoints;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Physics::Get()->DebugContactPoints = gv.AsBoolean();
                }
            ),

            REFLECTION_PROPERTY(
                "DebugSpatialHeat",
                Boolean,
                [](void*) -> Reflection::GenericValue
                {
                    return Physics::Get()->DebugSpatialHeat;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Physics::Get()->DebugSpatialHeat = gv.AsBoolean();
                }
            ),
        };

        return props;
    };
};

static PhysicsServiceManager Instance;
