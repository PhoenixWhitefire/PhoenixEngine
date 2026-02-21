// RigidBody.hpp, 22/01/2026

#pragma once

#include "datatype/GameObject.hpp"
#include "component/Transform.hpp"

enum class EnCollisionType : uint8_t
{
    Cube,          // Cube
    Sphere,        // Sphere
    Hulls,         // Hulls file
    MeshComponent  // Use the mesh from the attached `Mesh` component directly (usually a bad idea)
};

struct EcRigidBody : public Component<EntityComponent::RigidBody>
{
    void RecomputeAabb();
    void SetHullsFile(const std::string&);

    glm::vec3 LinearVelocity;
    glm::vec3 AngularVelocity;
    glm::vec3 NetForce;

    float Mass = 1.f;
	float Density = 1.f;
	float Friction = 50.f;
    float Restitution = 0.2f;
    float GravityFactor = 1.f;

    struct
	{
		glm::vec3 Position;
		glm::vec3 Size = { 1.f, 1.f, 1.f };
	} CollisionAabb;

    std::vector<glm::vec3> SpatialHashPoints;
	ObjectRef PrevWorkspace;
    ObjectRef Object;
    EcTransform* CurTransform; // Only used during the Physics phase

    std::string HullsFile;
    struct Hull
    {
        glm::mat4 Transform = glm::mat4(1.f);
        uint32_t MeshId = UINT32_MAX;
    };
    std::vector<Hull> Hulls;

    EnCollisionType CollisionType = EnCollisionType::Cube;

    bool PhysicsDynamics = false;
	bool PhysicsCollisions = true;
    bool PhysicsRotations = false;

    bool Valid = true;
};
