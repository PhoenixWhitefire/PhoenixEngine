#pragma once

#include <glm/mat4x4.hpp>

#include "datatype/GameObject.hpp"

struct EcTransform : public Component<EntityComponent::Transform>
{
    void SetWorldTransform(const glm::mat4&);
	void SetWorldSize(const glm::vec3&);
    void RecomputeTransformTree();

    // world-space
    glm::mat4 Transform{ 1.f };
    glm::vec3 Size{ 1.f, 1.f, 1.f };

    // local-space (relative to world transform of nearest Transform ancestor)
    glm::mat4 LocalTransform{ 1.f };
    glm::vec3 LocalSize{ 1.f, 1.f, 1.f };
    
    ObjectRef Object;
    bool Valid = true;
};
