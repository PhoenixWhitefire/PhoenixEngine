// Renderer service, 29/01/2026
#include "datatype/GameObject.hpp"

struct EcRendererService : public Component<EntityComponent::Renderer>
{
    bool Valid = true;
};
