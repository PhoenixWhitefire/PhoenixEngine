// Input service GameObject component

#pragma once

#include <string>

#include "datatype/GameObject.hpp"

struct EcInputService : public Component<EntityComponent::InputService>
{
    bool Valid = true;
};