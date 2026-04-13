// Collections service - tagging objects, 31/01/2026
#pragma once

#include "datatype/ComponentBase.hpp"

struct EcCollections : public Component<EntityComponent::Collections>
{
    ReflectorRef Reference;
    bool Valid = true;
};

class CollectionsComponentManager : public ComponentManager<EcCollections>
{
public:
    const Reflection::StaticMethodMap& GetMethods() override;
};
