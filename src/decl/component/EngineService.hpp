// Engine service GameObject component

#pragma once

#include <string>

#include "datatype/ComponentBase.hpp"

struct LogMessageType_ {
    enum LMT {
        None = 0,
        Info,
        Warning,
        Error
    };
};

using LogMessageType = LogMessageType_::LMT;

struct EcEngine : public Component<EntityComponent::Engine>
{
    static void SignalNewLogMessage(LogMessageType Type, const std::string_view& Message, const std::string_view& ExtraTags);

    std::vector<Reflection::EventCallback> OnMessageLoggedCallbacks;
    bool Valid = true;
};