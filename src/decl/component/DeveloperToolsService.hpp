// DeveloperTools service, 31/03/2026
#pragma once

#include "datatype/ComponentBase.hpp"

struct EcDeveloperToolsService : public Component<EntityComponent::DeveloperTools>
{
    bool Valid = true;
};

class DeveloperToolsComponentManager : public ComponentManager<EcDeveloperToolsService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
    const Reflection::StaticMethodMap& GetMethods() override;
};
