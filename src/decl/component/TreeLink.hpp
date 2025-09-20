// Precious hack
// Tells the Engine that it should pretend that the children of the Target is the children of
// the node while traversing the hierarchy to render the Scene and simulate physics.
// Replaceable when multi-datamodel is implemented

#include "datatype/GameObject.hpp"

struct EcTreeLink
{
    ObjectRef Target;
    bool Scripting = true;

    ObjectRef Object;
    bool Valid = true;
    
    static const EntityComponent Type = EntityComponent::TreeLink;
};
