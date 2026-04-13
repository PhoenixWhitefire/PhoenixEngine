// Network service, 22/02/2026
#pragma once

#include "datatype/ComponentBase.hpp"

struct EcNetworkService : public Component<EntityComponent::NetworkService>
{
    bool Valid = true;
};

class NetworkComponentManager : public ComponentManager<EcNetworkService>
{
public:
    const Reflection::StaticMethodMap& GetMethods() override;
};
