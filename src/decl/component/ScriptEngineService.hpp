// ScriptEngine service, 10/05/2026
#pragma once

#include "datatype/ComponentBase.hpp"

struct EcScriptEngineService : Component<EntityComponent::ScriptEngine>
{
    static void SignalBreakpointMoved(const std::vector<Reflection::GenericValue>&);
    std::vector<Reflection::EventConnection> BreakpointMovedCallbacks;

    bool Valid = true;
};

class ScriptEngineComponentManager : public ComponentManager<EcScriptEngineService>
{
    const Reflection::StaticMethodMap& GetMethods() override;
    const Reflection::StaticEventMap& GetEvents() override;
};
