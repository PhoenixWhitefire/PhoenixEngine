// Network service, 22/02/2026
#include "datatype/ComponentBase.hpp"

struct EcNetworkService : public Component<EntityComponent::NetworkService>
{
    bool Valid = true;
};
