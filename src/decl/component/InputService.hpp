// Input service GameObject component

#pragma once

#include <string>

#include "datatype/GameObject.hpp"

struct EcPlayerInput : public Component<EntityComponent::PlayerInput>
{
    bool Valid = true;
};