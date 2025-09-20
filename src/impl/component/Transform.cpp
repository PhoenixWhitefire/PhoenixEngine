#include "component/Transform.hpp"
#include "component/Mesh.hpp"
#include "datatype/GameObject.hpp"

class TransformsManager : ComponentManager<EcTransform>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
        m_Components.back().Object = Object;

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
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
};

static inline TransformsManager Instance{};
