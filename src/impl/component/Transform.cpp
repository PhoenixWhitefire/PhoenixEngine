#include "component/Transform.hpp"
#include "component/Mesh.hpp"
#include "datatype/GameObject.hpp"

class TransformsManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) final
    {
        m_Components.emplace_back();
        m_Components.back().Object = Object;

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() final
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcTransform& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

    virtual void* GetComponent(uint32_t Id) final
    {
        return &m_Components[Id];
    }

    virtual void DeleteComponent(uint32_t Id) final
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth

        m_Components[Id].Object.Invalidate();
    }

    virtual void Shutdown() final
    {
        for (size_t i = 0; i < m_Components.size(); i++)
            DeleteComponent(i);
    }

    virtual const Reflection::PropertyMap& GetProperties() final
    {
        static const Reflection::PropertyMap props = 
        {
            EC_PROP(
                "Transform",
                Matrix,
                EC_GET_SIMPLE(EcTransform, Transform),
                [](void* p, const Reflection::GenericValue& gv)
                {
                    EcTransform* ct = static_cast<EcTransform*>(p);
                    ct->Transform = gv.AsMatrix();

                    if (EcMesh* cm = ct->Object->GetComponent<EcMesh>())
                        cm->RecomputeAabb();
                }
            ),

            EC_PROP(
                "Size",
                Vector3,
                EC_GET_SIMPLE(EcTransform, Size),
                [](void* p, const Reflection::GenericValue& gv)
                {
                    EcTransform* ct = static_cast<EcTransform*>(p);
                    ct->Size = gv.AsVector3();

                    if (EcMesh* cm = ct->Object->GetComponent<EcMesh>())
                        cm->RecomputeAabb();
                }
            )
        };

        return props;
    }

    virtual const Reflection::FunctionMap& GetFunctions() final
    {
        static const Reflection::FunctionMap funcs = {};
        return funcs;
    }

    TransformsManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::Transform] = this;
    }

private:
    std::vector<EcTransform> m_Components;
};

static inline TransformsManager Instance{};
