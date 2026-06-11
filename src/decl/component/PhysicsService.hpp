// Physics Service, 23/01/2026
#pragma once

#include "geometry/Physics.hpp"
#include "datatype/ComponentBase.hpp"

struct EcPhysicsService : Component<EntityComponent::PhysicsService>
{
    glm::vec3 Gravity = { 0.f, -50.f, 0.f };
    double Timescale = 1.0;

    bool Simulating = true;
    bool DebugCollisionAabbs = false;
    bool DebugContactPoints = false;
    bool DebugSpatialHeat = false;

    bool IsServiceInstance = false;
    bool Valid = true;
};

class PhysicsComponentManager : public ComponentManager<EcPhysicsService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
    const Reflection::StaticMethodMap& GetMethods() override;
    uint32_t CreateComponent(GameObject*) override;

    void BindService(uint32_t) override;
    void UnbindService() override;

    EcPhysicsService* GetService() const;
    ObjectHandle ServiceInstance;
};
