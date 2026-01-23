// Physics Service, 23/01/2026

#pragma once

#include "geometry/Physics.hpp"
#include "datatype/GameObject.hpp"

struct EcPhysicsService : Component<EntityComponent::PhysicsService>
{
    bool Valid = true;
};
