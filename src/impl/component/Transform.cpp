#include <tracy/Tracy.hpp>

#include "component/Transform.hpp"
#include "component/RigidBody.hpp"
#include "datatype/GameObject.hpp"

static void recomputeAabbRecursive(const ObjectHandle& Object)
{
    if (EcRigidBody* crb = Object->FindComponent<EcRigidBody>())
        crb->RecomputeAabb();

    Object->ForEachChild([](const ObjectHandle& Child) -> bool
    {
        recomputeAabbRecursive(Child);
        return true;
    });
}

static void recomputeChildrenWorldTransformsRecursive(const ObjectHandle& Object)
{
    EcTransform* pct = Object->FindComponent<EcTransform>();

    Object->ForEachChild([Object, pct](const ObjectHandle& Child) -> bool
    {
        if (EcTransform* ct = Child->FindComponent<EcTransform>())
        {
            glm::mat4 offsetLocal = ct->LocalTransform;
            offsetLocal[3] = glm::vec4(glm::vec3(ct->LocalTransform[3]) * pct->Size, 1.f);

            ct->Transform = pct->Transform * offsetLocal;
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

uint32_t TransformComponentManager::CreateComponent(GameObject* Object)
{
    uint32_t id = ComponentManager<EcTransform>::CreateComponent(Object);
    m_Components[id].Object = Object;
    m_Components[id].RecomputeTransformTree();

    return id;
}

const Reflection::StaticPropertyMap& TransformComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        { "LocalTransform", Reflection::PropertyDescriptor{
            .Name = "LocalTransform",
            .Get = REFLECTION_PROPERTY_GET_SIMPLE(EcTransform, LocalTransform),
            .Set = (Reflection::PropertySetter)[](void* p, const Reflection::GenericValue& gv)
            {
                ZoneScoped;
                EcTransform* ct = static_cast<EcTransform*>(p);
                glm::mat4 prevTrans = ct->Transform;

                ct->LocalTransform = gv.AsMatrix();
                ct->RecomputeTransformTree();

                REFLECTION_SIGNAL_EVENT(ct->OnScriptMovedCallbacks, prevTrans, ct->Transform);
            },
            .Type = Reflection::ValueType::Matrix,
            .ParallelReadSafe = false, // Physics
        } },

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

        { "Transform", Reflection::PropertyDescriptor{
            .Name = "Transform",
            .Get = REFLECTION_PROPERTY_GET_SIMPLE(EcTransform, Transform),
            .Set = (Reflection::PropertySetter)[](void* p, const Reflection::GenericValue& gv)
            {
                ZoneScoped;
                EcTransform* ct = static_cast<EcTransform*>(p);
                glm::mat4 prevTrans = ct->Transform;
                ct->SetWorldTransform(gv.AsMatrix());

                REFLECTION_SIGNAL_EVENT(ct->OnScriptMovedCallbacks, prevTrans, ct->Transform);
            },
            .Type = Reflection::ValueType::Matrix,
            .Serializes = false,
            .ParallelReadSafe = false, // Physics
        } },

        { "Size", Reflection::PropertyDescriptor{
            .Name = "Size",
            .Get = (Reflection::PropertyGetter)[](void* p)->Reflection::GenericValue
            {
                return static_cast<EcTransform*>(p)->Size;
            },
            .Set = (Reflection::PropertySetter)[](void* p, const Reflection::GenericValue& gv)
            {
                ZoneScoped;
                    
                EcTransform* ct = static_cast<EcTransform*>(p);
                ct->SetWorldSize(gv.AsVector3());
            },
            .Type = Reflection::ValueType::Vector3,
            .Serializes = false,
        } }
    };

    return props;
}

const Reflection::StaticEventMap& TransformComponentManager::GetEvents()
{
    static const Reflection::StaticEventMap events = {
        REFLECTION_EVENT(EcTransform, OnScriptMoved, Reflection::ValueType::Matrix, Reflection::ValueType::Matrix),
    };

    return events;
}

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
    ZoneScoped;

    recomputeWorldTransforms(this);
    recomputeAabbRecursive(Object);
}
