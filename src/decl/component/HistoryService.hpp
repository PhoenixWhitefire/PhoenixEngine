// HistoryService.hpp, 17/12/2025 - Interfacing with History backend
#pragma once

#include "datatype/ComponentBase.hpp"

struct EcHistoryService : Component<EntityComponent::History>
{
    bool Valid = true;
};

class HistoryComponentManager : ComponentManager<EcHistoryService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
    const Reflection::StaticMethodMap& GetMethods() override;
};
