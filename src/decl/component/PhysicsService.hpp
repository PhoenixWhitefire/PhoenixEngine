// Physics Service, 23/01/2026
#pragma once

#include "geometry/Physics.hpp"
#include "datatype/ComponentBase.hpp"

struct EcPhysicsService : Component<EntityComponent::PhysicsService>
{
    bool Valid = true;
};

class PhysicsComponentManager : public ComponentManager<EcPhysicsService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
};
