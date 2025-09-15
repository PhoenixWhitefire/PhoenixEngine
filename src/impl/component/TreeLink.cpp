#include "component/TreeLink.hpp"

class TreeLinkManager : public ComponentManager<EcTreeLink>
{
    uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
		m_Components.back().Object = Object;

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    void DeleteComponent(uint32_t Id) override
    {
        m_Components[Id].Object.~GameObjectRef();
        m_Components[Id].Target.~GameObjectRef();

        ComponentManager<EcTreeLink>::DeleteComponent(Id);
    }

    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props =
        {
            { "Target",
                {
                    (Reflection::ValueType)((uint8_t)Reflection::ValueType::GameObject + (uint8_t)Reflection::ValueType::Null),
                    [](void* p)
                    {
                        EcTreeLink* tl = static_cast<EcTreeLink*>(p);
                        return GameObject::s_ToGenericValue(GameObject::GetObjectById(tl->Target.m_TargetId));
                    },
                    [](void* p, const Reflection::GenericValue& gv)
                    {
                        EcTreeLink* tl = static_cast<EcTreeLink*>(p);
                        GameObject* newTarget = GameObject::FromGenericValue(gv);

                        if (!newTarget)
                        {
                            // i need to re-write `GameObjectRef` completely
                            if (GameObject* t = GameObject::GetObjectById(tl->Target.m_TargetId))
                                t->DecrementHardRefs();

                            tl->Target.m_TargetId = PHX_GAMEOBJECT_NULL_ID;
                            return;
                        }

                        if (tl->Object->IsDescendantOf(newTarget))
                            RAISE_RT("Cannot make a TreeLink target an ancestor of itself");

                        if (newTarget->GetComponent<EcTreeLink>())
                            // code to prevent infinite recursion is too complicated for me to care right now
                            RAISE_RT("TreeLink cannot target another TreeLink");

                        tl->Target = GameObject::FromGenericValue(gv);
                    }
                }
            },

            EC_PROP_SIMPLE(EcTreeLink, Scripting, Boolean)
        };

        return props;
    }
};
static TreeLinkManager Instance{};
