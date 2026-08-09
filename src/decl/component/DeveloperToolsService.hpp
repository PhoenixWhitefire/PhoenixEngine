// DeveloperTools service, 31/03/2026
#pragma once

#include "Reflection.hpp"
#include "datatype/ComponentBase.hpp"

struct EcDeveloperToolsService : public Component<EntityComponent::DeveloperTools>
{
    static inline std::vector<Reflection::EventConnection> BreakpointUpdatedCallbacks;
    static inline std::vector<Reflection::EventConnection> BreakpointRemovedCallbacks;
    static inline std::vector<Reflection::EventConnection> DebuggerRequestedStopCallbacks;

    bool Valid = true;
};

class DeveloperToolsComponentManager : public ComponentManager<EcDeveloperToolsService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
    const Reflection::StaticMethodMap& GetMethods() override;
    const Reflection::StaticEventMap& GetEvents() override;

    void BindService(uint32_t) override;
    void UnbindService() override;
};
