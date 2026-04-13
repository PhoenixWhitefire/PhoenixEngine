// Logging service component, 02/03/2026
#include <tracy/Tracy.hpp>

#include "component/LoggingService.hpp"
#include "Utilities.hpp"

const Reflection::StaticMethodMap& LoggingComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap methods = {
        { "Write", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, Reflection::ValueType::Integer, REFLECTION_OPTIONAL(String) },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                std::string_view message = inputs[0].AsStringView();
                int64_t type = inputs[1].AsInteger();
                std::string_view tags = inputs.size() > 2 ? inputs[2].AsStringView() : "";

                if (type < 0 || type > (int64_t)Logging::MessageType::Error)
                    RAISE_RTF("Invalid log message type '{}'", type);

                Logging::MessageType mt = (Logging::MessageType)type;
                Log.Write(message, mt, tags);

                return {};
            }
        } }
    };

    return methods;
}

const Reflection::StaticEventMap& LoggingComponentManager::GetEvents()
{
    static const Reflection::StaticEventMap events = {
        REFLECTION_EVENT(
            EcLoggingService,
            OnMessaged,
            Reflection::ValueType::Integer, Reflection::ValueType::String, Reflection::ValueType::String, Reflection::ValueType::Any
        )
    };

    return events;
}

void LoggingComponentManager::SignalNewLogMessage(Logging::MessageType MessageType, const std::string_view& Message, const std::string_view& ExtraTags, const Reflection::GenericValue& Value)
{
    for (const EcLoggingService& cl : Instance.m_Components)
    {
        if (cl.Valid)
            REFLECTION_SIGNAL_EVENT(cl.OnMessagedCallbacks, (int)MessageType, Message, ExtraTags, Value);
    }
}
