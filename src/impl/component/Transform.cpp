#include <tracy/Tracy.hpp>

#include "component/Transform.hpp"
#include "component/Mesh.hpp"
#include "datatype/GameObject.hpp"

static void recomputeAabbRecursive(GameObject* Object)
{
    if (EcMesh* cm = Object->FindComponent<EcMesh>())
        cm->RecomputeAabb();

    Object->ForEachChild([](GameObject* Child) -> bool
    {
        recomputeAabbRecursive(Child);
        return true;
    });
}

static void recomputeChildrenWorldTransformsRecursive(GameObject* Object)
{
    EcTransform* pct = Object->FindComponent<EcTransform>();

    Object->ForEachChild([Object, pct](GameObject* Child) -> bool
    {
        if (EcTransform* ct = Child->FindComponent<EcTransform>())
        {
            ct->Transform = pct->Transform * ct->LocalTransform;
            ct->Size = pct->Size * ct->LocalSize;
        }

        recomputeChildrenWorldTransformsRecursive(Child);
        return true;
    });
}

static void recomputeWorldTransforms(EcTransform* ct)
{
    ct->Transform = ct->LocalTransform;
    ct->Size = ct->LocalSize;

    GameObject* parent = ct->Object->GetParent();

    while (parent)
    {
        if (EcTransform* pct = parent->FindComponent<EcTransform>())
        {
            ct->Transform = pct->Transform * ct->LocalTransform; // parent should already have up-to-date World Transforms
            ct->Size = pct->Size * ct->LocalSize;
            break;
        }
        parent = parent->GetParent();
    }

    recomputeChildrenWorldTransformsRecursive(ct->Object);
}

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
                    ZoneScoped;

                    EcTransform* ct = static_cast<EcTransform*>(p);
                    ct->SetWorldTransform(gv.AsMatrix());
                    ct->RecomputeTransformTree();
                }
            ),

            EC_PROP(
                "Size",
                Vector3,
                EC_GET_SIMPLE(EcTransform, Size),
                [](void* p, const Reflection::GenericValue& gv)
                {
                    ZoneScoped;

                    EcTransform* ct = static_cast<EcTransform*>(p);
                    ct->SetWorldSize(gv.AsVector3());
                    ct->RecomputeTransformTree();
                }
            ),

            { "LocalTransform", {
                Reflection::ValueType::Matrix, 
                (Reflection::PropertyGetter)[](void* p)->Reflection::GenericValue
                {
                    return static_cast<EcTransform*>(p)->LocalTransform;
                },
                (Reflection::PropertySetter)[](void* p, const Reflection::GenericValue& gv)
                {
                    ZoneScoped;
                    
                    EcTransform* ct = static_cast<EcTransform*>(p);
                    ct->LocalTransform = gv.AsMatrix();
                    ct->RecomputeTransformTree();
                },
                false
            } },

            { "LocalSize", {
                Reflection::ValueType::Vector3, 
                (Reflection::PropertyGetter)[](void* p)->Reflection::GenericValue
                {
                    return static_cast<EcTransform*>(p)->LocalSize;
                },
                (Reflection::PropertySetter)[](void* p, const Reflection::GenericValue& gv)
                {
                    ZoneScoped;
                    
                    EcTransform* ct = static_cast<EcTransform*>(p);
                    ct->LocalSize = gv.AsVector3();
                    ct->RecomputeTransformTree();
                },
                false
            } }
        };

        return props;
    }
};

static inline TransformsManager Instance{};

void EcTransform::SetWorldTransform(const glm::mat4& NewWorldTrans)
{
    EcTransform* parent = Object->GetParent() ? Object->GetParent()->FindComponent<EcTransform>() : nullptr;

    LocalTransform = parent ? (NewWorldTrans * glm::inverse(parent->Transform)) : NewWorldTrans;
    Transform = NewWorldTrans;
}

void EcTransform::SetWorldSize(const glm::vec3& NewWorldSize)
{
    EcTransform* parent = Object->GetParent() ? Object->GetParent()->FindComponent<EcTransform>() : nullptr;

    LocalSize = parent ? (NewWorldSize / parent->Size) : NewWorldSize;
    Size = NewWorldSize;
}

void EcTransform::RecomputeTransformTree()
{
    recomputeWorldTransforms(this);
    recomputeAabbRecursive(Object);
}
