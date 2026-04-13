// Engine service GameObject component
#pragma once

#include <string>

#include "datatype/ComponentBase.hpp"
#include "Log.hpp"

struct EcEngine : public Component<EntityComponent::Engine>
{
    bool Valid = true;
};

class EngineComponentManager : public ComponentManager<EcEngine>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
};
