// Input service GameObject component
#pragma once

#include <string>

#include "datatype/ComponentBase.hpp"

struct EcPlayerInput : public Component<EntityComponent::PlayerInput>
{
    static inline std::vector<Reflection::EventConnection> KeyEventCallbacks;
    static inline std::vector<Reflection::EventConnection> MouseButtonEventCallbacks;
    static inline std::vector<Reflection::EventConnection> ScrollEventCallbacks;

    bool Valid = true;
};

class PlayerInputComponentManager : public ComponentManager<EcPlayerInput>
{
    const Reflection::StaticPropertyMap& GetProperties() override;
    const Reflection::StaticMethodMap& GetMethods() override;
    const Reflection::StaticEventMap& GetEvents() override;
    void Shutdown() override;
};
