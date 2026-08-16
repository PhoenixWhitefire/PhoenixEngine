// Input service GameObject component
#pragma once

#include "datatype/ComponentBase.hpp"

struct EcPlayerInput : public Component<EntityComponent::PlayerInput>
{
    static void SignalKeyEvent(const std::vector<Reflection::GenericValue>&);
    static void SignalMouseButtonEvent(const std::vector<Reflection::GenericValue>&);
    static void SignalScrollEvent(const std::vector<Reflection::GenericValue>&);

    std::vector<Reflection::EventConnection> KeyEventCallbacks;
    std::vector<Reflection::EventConnection> MouseButtonEventCallbacks;
    std::vector<Reflection::EventConnection> ScrollEventCallbacks;

    bool Valid = true;
};

class PlayerInputComponentManager : public ComponentManager<EcPlayerInput>
{
    const Reflection::StaticPropertyMap& GetProperties() override;
    const Reflection::StaticMethodMap& GetMethods() override;
    const Reflection::StaticEventMap& GetEvents() override;
    void Shutdown() override;
};
