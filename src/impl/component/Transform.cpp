#include <tracy/Tracy.hpp>

#include "component/Transform.hpp"
#include "component/RigidBody.hpp"
#include "datatype/GameObject.hpp"

static void recomputeAabbRecursive(GameObject* Object)
{
    if (EcRigidBody* crb = Object->FindComponent<EcRigidBody>())
        crb->RecomputeAabb();

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
        static const Reflection::StaticPropertyMap props = {
            REFLECTION_PROPERTY(
                "LocalTransform",
                Matrix,
                REFLECTION_PROPERTY_GET_SIMPLE(EcTransform, LocalTransform),
                [](void* p, const Reflection::GenericValue& gv)
                {
                    ZoneScoped;

                    EcTransform* ct = static_cast<EcTransform*>(p);
                    ct->LocalTransform = gv.AsMatrix();
                    ct->RecomputeTransformTree();
                }
            ),

            REFLECTION_PROPERTY(
                "LocalSize",
                Vector3,
                REFLECTION_PROPERTY_GET_SIMPLE(EcTransform, LocalSize),
                [](void* p, const Reflection::GenericValue& gv)
                {
                    ZoneScoped;

                    EcTransform* ct = static_cast<EcTransform*>(p);
                    ct->LocalSize = gv.AsVector3();
                    ct->RecomputeTransformTree();
                }
            ),

            { "Transform", {
                "Transform",
                Reflection::ValueType::Matrix,
                REFLECTION_PROPERTY_GET_SIMPLE(EcTransform, Transform),
                (Reflection::PropertySetter)[](void* p, const Reflection::GenericValue& gv)
                {
                    ZoneScoped;
                    
                    EcTransform* ct = static_cast<EcTransform*>(p);
                    ct->SetWorldTransform(gv.AsMatrix());
                    ct->RecomputeTransformTree();
                },
                false
            } },

            { "Size", {
                "Size",
                Reflection::ValueType::Vector3, 
                (Reflection::PropertyGetter)[](void* p)->Reflection::GenericValue
                {
                    return static_cast<EcTransform*>(p)->LocalSize;
                },
                (Reflection::PropertySetter)[](void* p, const Reflection::GenericValue& gv)
                {
                    ZoneScoped;
                    
                    EcTransform* ct = static_cast<EcTransform*>(p);
                    ct->SetWorldSize(gv.AsVector3());
                    ct->RecomputeTransformTree();
                },
                false
            } }
        };

        return props;
    }
};

static inline TransformsManager Instance;

void EcTransform::SetWorldTransform(const glm::mat4& NewWorldTrans)
{
    EcTransform* parent = Object->GetParent() ? Object->GetParent()->FindComponent<EcTransform>() : nullptr;

    LocalTransform = parent ? (glm::inverse(parent->Transform) * NewWorldTrans) : NewWorldTrans;
    Transform = NewWorldTrans;

    RecomputeTransformTree();
}

void EcTransform::SetWorldSize(const glm::vec3& NewWorldSize)
{
    EcTransform* parent = Object->GetParent() ? Object->GetParent()->FindComponent<EcTransform>() : nullptr;

    LocalSize = parent ? (NewWorldSize / parent->Size) : NewWorldSize;
    Size = NewWorldSize;

    RecomputeTransformTree();
}

void EcTransform::RecomputeTransformTree()
{
    recomputeWorldTransforms(this);
    recomputeAabbRecursive(Object);
}
