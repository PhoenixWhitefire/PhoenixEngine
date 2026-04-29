#include "component/TreeLink.hpp"

uint32_t TreeLinkComponentManager::CreateComponent(GameObject* Object)
{
    uint32_t id = ComponentManager<EcTreeLink>::CreateComponent(Object);
	m_Components[id].Object = Object;

    return id;
}

const Reflection::StaticPropertyMap& TreeLinkComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props =
    {
        { "Target", {
            "Target",
            REFLECTION_OPTIONAL(GameObject),
            [](void* p)
            {
                EcTreeLink* tl = static_cast<EcTreeLink*>(p);
                return GameObject::s_ToGenericValue(tl->Target.Referred());
            },
            [](void* p, const Reflection::GenericValue& gv)
            {
                EcTreeLink* tl = static_cast<EcTreeLink*>(p);
                GameObject* newTarget = GameObjectManager::Get()->FromGenericValue(gv);

                if (!newTarget)
                {
                    tl->Target = nullptr;
                    return;
                }

                if (tl->Object->IsDescendantOf(newTarget))
                    RAISE_RT("Cannot make a TreeLink target an ancestor of itself");

                if (newTarget->FindComponent<EcTreeLink>())
                    // code to prevent infinite recursion is too complicated for me to care right now
                    RAISE_RT("TreeLink cannot target another TreeLink");

                tl->Target = GameObjectManager::Get()->FromGenericValue(gv);
            }
        } }
    };

    return props;
}
