#include <tracy/Tracy.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include "component/Transform.hpp"
#include "component/RigidBody.hpp"
#include "datatype/GameObject.hpp"
#include "geometry/DecomposeTRS.hpp"

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
            ct->Transform = pct->Transform * ct->LocalTransform;

        recomputeChildrenWorldTransformsRecursive(Child);
        return true;
    });
}

static void recomputeWorldTransforms(EcTransform* ct)
{
    ct->Transform = ct->LocalTransform;

    GameObject* parent = ct->Object->GetParent();

    while (parent)
    {
        if (EcTransform* pct = parent->FindComponent<EcTransform>())
        {
            ct->Transform = pct->Transform * ct->LocalTransform; // parent should already have up-to-date World Transforms
            break;
        }
        parent = parent->GetParent();
    }

    recomputeChildrenWorldTransformsRecursive(ct->Object);
}

uint32_t TransformComponentManager::CreateComponent(GameObject* Object)
{
    uint32_t id = ComponentManager<EcTransform>::CreateComponent(Object);
    Components[id].Object = Object;
    Components[id].RecomputeTransformTree();

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
                glm::mat4 prevTrans = ct->LocalTransform;
                ct->SetLocalTransform(gv.AsMatrix());

                Reflection::SignalEvent(ct->OnScriptMovedCallbacks, { prevTrans, ct->Transform }, "Transform.OnScriptMoved");
            },
            .Type = Reflection::ValueType::Matrix,
            .ParallelReadSafe = false, // Physics
        } },

        { "Transform", Reflection::PropertyDescriptor{
            .Name = "Transform",
            .Get = REFLECTION_PROPERTY_GET_SIMPLE(EcTransform, Transform),
            .Set = (Reflection::PropertySetter)[](void* p, const Reflection::GenericValue& gv)
            {
                ZoneScoped;

                EcTransform* ct = static_cast<EcTransform*>(p);
                glm::mat4 prevTrans = ct->Transform;
                ct->SetWorldTransform(gv.AsMatrix());

                Reflection::SignalEvent(ct->OnScriptMovedCallbacks, { prevTrans, ct->Transform }, "Transform.OnScriptMoved");
            },
            .Type = Reflection::ValueType::Matrix,
            .Serializes = false,
            .ParallelReadSafe = false, // Physics
        } },
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
    glm::vec3 translation = {};
    glm::quat rotation = {};
    DecomposeTRS(Transform, &translation, &rotation, nullptr);

    Transform = glm::translate(glm::mat4(1.f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.f), NewWorldSize);

    EcTransform* parent = Object->GetParent() ? Object->GetParent()->FindComponent<EcTransform>() : nullptr;
    glm::vec3 parentSize = { 1.f, 1.f, 1.f };

    if (parent)
        DecomposeTRS(parent->Transform, nullptr, nullptr, &parentSize);

    glm::vec3 localSize = NewWorldSize / parentSize;
    SetLocalSize(localSize);
}

void EcTransform::SetLocalTransform(const glm::mat4& NewLocalTrans)
{
    LocalTransform = NewLocalTrans;
    RecomputeTransformTree();
}

void EcTransform::SetLocalSize(const glm::vec3& NewLocalSize)
{
    glm::vec3 translation = {};
    glm::quat rotation = {};
    DecomposeTRS(LocalTransform, &translation, &rotation, nullptr);

    LocalTransform = glm::translate(glm::mat4(1.f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.f), NewLocalSize);
    RecomputeTransformTree();
}

void EcTransform::RecomputeTransformTree()
{
    ZoneScoped;

    recomputeWorldTransforms(this);
    recomputeAabbRecursive(Object);
}
