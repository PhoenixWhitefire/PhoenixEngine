// Engine service GameObject component

#pragma once

#include <string>

#include "datatype/GameObject.hpp"

struct EcEngine : public Component<EntityComponent::Engine>
{
    bool Valid = true;
};