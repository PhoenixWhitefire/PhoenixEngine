// RigidBody.hpp, 22/01/2026

#pragma once

#include "datatype/GameObject.hpp"
#include "component/Transform.hpp"

enum class EnCollisionMode : uint8_t { Cube, MeshComponent };

struct EcRigidBody : public Component<EntityComponent::RigidBody>
{
    void RecomputeAabb();

    glm::vec3 LinearVelocity;
    glm::vec3 AngularVelocity;
    glm::vec3 NetForce;

    float Mass = 1.f;
	float Density = 1.f;
	float Friction = 0.3f;

    struct
	{
		glm::vec3 Position;
		glm::vec3 Size = { 1.f, 1.f, 1.f };
	} CollisionAabb;
	EnCollisionMode CollisionMode = EnCollisionMode::Cube;

    std::vector<glm::vec3> SpatialHashPoints;
	ObjectRef PrevWorkspace;
    ObjectRef Object;
    EcTransform* CurTransform; // Only used during the Physics phase

    bool PhysicsDynamics = false;
	bool PhysicsCollisions = true;

    bool Valid = true;
};
