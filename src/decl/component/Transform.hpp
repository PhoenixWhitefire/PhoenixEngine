#pragma once

#include <glm/mat4x4.hpp>

#include "datatype/GameObject.hpp"
#include "datatype/Vector3.hpp"

struct EcTransform
{
    glm::mat4 Transform{ 1.f };
    Vector3 Size{ 1.f, 1.f, 1.f };
    GameObjectRef Object;

    static inline EntityComponent Type = EntityComponent::Transform;
};
