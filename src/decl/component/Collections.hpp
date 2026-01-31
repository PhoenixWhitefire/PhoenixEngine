// Collections service - tagging objects, 31/01/2026
#include "datatype/ComponentBase.hpp"

struct EcCollections : public Component<EntityComponent::Collections>
{
    ReflectorRef Reference;
    bool Valid = true;
};
