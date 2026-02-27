// Engine service GameObject component

#pragma once

#include <string>

#include "datatype/ComponentBase.hpp"
#include "Log.hpp"

struct EcEngine : public Component<EntityComponent::Engine>
{
    static void SignalNewLogMessage(Logging::MessageType Type, const std::string_view& Message, const std::string_view& ExtraTags);

    std::vector<Reflection::EventCallback> OnMessageLoggedCallbacks;
    bool Valid = true;
};