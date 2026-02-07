// Input service GameObject component
#pragma once

#include <string>

#include "datatype/ComponentBase.hpp"

struct EcPlayerInput : public Component<EntityComponent::PlayerInput>
{
    static inline std::vector<Reflection::EventCallback> KeyEventCallbacks;
    static inline std::vector<Reflection::EventCallback> MouseButtonEventCallbacks;
    static inline std::vector<Reflection::EventCallback> ScrollEventCallbacks;

    bool Valid = true;
};