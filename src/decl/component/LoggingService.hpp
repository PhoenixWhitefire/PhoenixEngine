// Logging service component, 02/03/2026
#pragma once

#include "datatype/ComponentBase.hpp"
#include "Log.hpp"

struct EcLoggingService : public Component<EntityComponent::Logging>
{
    std::vector<Reflection::EventCallback> OnMessagedCallbacks;
    bool Valid = true;
};

class LoggingComponentManager : public ComponentManager<EcLoggingService>
{
public:
    const Reflection::StaticMethodMap& GetMethods() override;
    const Reflection::StaticEventMap& GetEvents() override;

    void SignalNewLogMessage(
        Logging::MessageType Type,
        const std::string_view& Message,
        const std::string_view& ExtraTags,
        const Reflection::GenericValue& Value = Reflection::GenericValue::Null()
    );
};
