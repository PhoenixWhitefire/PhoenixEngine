#include "component/Model.hpp"
#include "datatype/GameObject.hpp"

class ModelManager : public ComponentManager<EcModel>
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();

        if (!Object->GetComponentByType(EntityComponent::Transform))
            Object->AddComponent(EntityComponent::Transform);
        
        return static_cast<uint32_t>(m_Components.size() - 1);
    }
};

static inline ModelManager Instance{};
