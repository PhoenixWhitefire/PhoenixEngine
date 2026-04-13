// Renderer service, 29/01/2026
#pragma once

#include "datatype/ComponentBase.hpp"

struct EcRendererService : public Component<EntityComponent::Renderer>
{
    bool Valid = true;
};

class RendererComponentManager : public ComponentManager<EcRendererService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
    const Reflection::StaticMethodMap& GetMethods() override;
};
