#pragma once

#include <glm/mat4x4.hpp>

#include "datatype/GameObject.hpp"

struct EcTransform
{
    glm::mat4 Transform{ 1.f };
    glm::vec3 Size{ 1.f, 1.f, 1.f };
    ObjectRef Object;
    bool Valid = true;

    static inline EntityComponent Type = EntityComponent::Transform;
};
