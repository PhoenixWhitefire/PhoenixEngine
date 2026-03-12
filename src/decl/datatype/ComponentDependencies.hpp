// ComponentCommonDependencies, 02/03/2026 - Not actually *required* for Components, but people will usually want these going together
#pragma once

#include <vector>
#include <span>

#include "EntityComponent.hpp"

// I would use a `std::span`, but that didn't support initializer lists until C++ 26
// Then I tried to use a C-style array, but that is apparently a non-standard, forbidden, "compound literal"
// Oh well
static inline const std::unordered_map<EntityComponent, std::vector<EntityComponent>> s_ComponentCommonDependencies = {
    { EntityComponent::Camera,          { EntityComponent::Transform }                             },
    { EntityComponent::PointLight,      { EntityComponent::Transform }                             },
    { EntityComponent::SpotLight,       { EntityComponent::Transform }                             },
    { EntityComponent::Mesh,            { EntityComponent::RigidBody, EntityComponent::Transform } },
    { EntityComponent::Model,           { EntityComponent::Transform }                             },
    { EntityComponent::ParticleEmitter, { EntityComponent::Transform }                             },
    { EntityComponent::RigidBody,       { EntityComponent::Transform }                             },
    { EntityComponent::UIFrame,         { EntityComponent::UITransform }                           },
    { EntityComponent::UIImage,         { EntityComponent::UITransform }                           },
    { EntityComponent::UIButton,        { EntityComponent::UITransform, EntityComponent::UIFrame } }
};

const std::span<const EntityComponent> GetCommonDependenciesForComponent(EntityComponent ec);
