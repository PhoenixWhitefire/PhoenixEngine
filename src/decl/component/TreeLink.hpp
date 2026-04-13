// Precious hack
// Tells the Engine that it should pretend that the children of the Target is the children of
// the node while traversing the hierarchy to render the Scene and simulate physics.
// Replaceable when multi-datamodel is implemented
#pragma once

#include "datatype/GameObject.hpp"
#include "datatype/ComponentBase.hpp"

struct EcTreeLink : public Component<EntityComponent::TreeLink>
{
    ObjectRef Target;
    ObjectRef Object;
    bool Valid = true;
};

class TreeLinkComponentManager : public ComponentManager<EcTreeLink>
{
public:
    uint32_t CreateComponent(GameObject* Object) override;
    const Reflection::StaticPropertyMap& GetProperties() override;
};
