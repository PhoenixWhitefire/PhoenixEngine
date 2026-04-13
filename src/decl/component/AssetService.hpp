#pragma once

#include "datatype/ComponentBase.hpp"

struct EcAssetService : Component<EntityComponent::AssetManager>
{
    bool Valid = true;
};

class AssetServiceComponentManager : public ComponentManager<EcAssetService>
{
public:
    const Reflection::StaticMethodMap& GetMethods() override;
};
