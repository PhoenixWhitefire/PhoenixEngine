#include "component/Transformable.hpp"
#include "component/Mesh.hpp"
#include "datatype/GameObject.hpp"

class TransformablesManager : BaseComponentManager
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

        for (const EcTransformable& t : m_Components)
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
    }

    virtual const Reflection::PropertyMap& GetProperties() final
    {
        static const Reflection::PropertyMap props = 
        {
            EC_PROP(
                "Transform",
                Matrix,
                EC_GET_SIMPLE(EcTransformable, Transform),
                [](void* p, const Reflection::GenericValue& gv)
                {
                    EcTransformable* ct = static_cast<EcTransformable*>(p);
                    ct->Transform = gv.AsMatrix();

                    if (EcMesh* cm = ct->Object->GetComponent<EcMesh>())
                        cm->RecomputeAabb();
                }
            ),

            EC_PROP(
                "Size",
                Vector3,
                EC_GET_SIMPLE_TGN(EcTransformable, Size),
                [](void* p, const Reflection::GenericValue& gv)
                {
                    EcTransformable* ct = static_cast<EcTransformable*>(p);
                    ct->Size = Vector3(gv);

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

    TransformablesManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::Transformable] = this;
    }

private:
    std::vector<EcTransformable> m_Components;
};

static inline TransformablesManager Instance{};
