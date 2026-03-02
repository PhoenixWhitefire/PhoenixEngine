// Logging service component, 02/03/2026
#pragma once

#include "datatype/ComponentBase.hpp"
#include "Log.hpp"

struct EcLoggingService : public Component<EntityComponent::Logging>
{
    static void SignalNewLogMessage(
        Logging::MessageType Type,
        const std::string_view& Message,
        const std::string_view& ExtraTags,
        const Reflection::GenericValue& Value = Reflection::GenericValue::Null()
    );

    std::vector<Reflection::EventCallback> OnMessagedCallbacks;
    bool Valid = true;
};
