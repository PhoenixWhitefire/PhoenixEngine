// HistoryService.hpp, 17/12/2025 - Interfacing with History backend

#include "datatype/GameObject.hpp"

struct EcHistoryService : Component<EntityComponent::History>
{
    bool Valid = true;
};

