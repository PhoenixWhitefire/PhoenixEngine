// Engine service GameObject component

#pragma once

#include <string>

#include "datatype/GameObject.hpp"

struct EcEngine : public Component<EntityComponent::Engine>
{
    static void SignalNewLogMessage(const std::string_view&);

    std::vector<Reflection::EventCallback> OnMessageLoggedCallbacks;
    bool Valid = true;
};