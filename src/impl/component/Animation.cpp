#include <vector>
#include <Vendor/nljson.hpp>
#include <tracy/Tracy.hpp>

#include "component/Animation.hpp"
#include "component/Transform.hpp"
#include "component/Bone.hpp"
#include "component/Mesh.hpp"
#include "datatype/GameObject.hpp"
#include "asset/Binary.hpp"
#include "FileRW.hpp"

const Reflection::StaticPropertyMap& AnimationComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY(
            "Animation",
            String,
            REFLECTION_PROPERTY_GET_SIMPLE(EcAnimation, Animation),
            [](void* p, const Reflection::GenericValue& gv)
            {

                static_cast<EcAnimation*>(p)->SetAnimation(gv.AsString());
            }
        ),

        REFLECTION_PROPERTY_SIMPLE(EcAnimation, Time, Double),
        REFLECTION_PROPERTY_SIMPLE(EcAnimation, Weight, Double),
        REFLECTION_PROPERTY_SIMPLE(EcAnimation, Looped, Boolean),

        { "Playing", Reflection::PropertyDescriptor{
            .Name = "Playing",
            .Get = REFLECTION_PROPERTY_GET_SIMPLE(EcAnimation, Playing),
            .Set = [](void* p, const Reflection::GenericValue& gv)
            {
                EcAnimation* ea = static_cast<EcAnimation*>(p);
                bool playing = gv.AsBoolean();

                if (playing)
                {
                    GameObject* parent = ea->Object->GetParent();
                    while (parent)
                    {
                        if (EcAnimator* animator = parent->FindComponent<EcAnimator>())
                        {
                            if (ea->AnimationId == UINT32_MAX)
                                ea->SetAnimation(ea->Animation);

                            animator->LoadAnimation(ea->Object, ea->AnimationId);
                            break;
                        }

                        parent = parent->GetParent();
                    }

                    ea->Playing = true;
                }
                else
                    ea->Playing = false;
            },
            .Type = Reflection::ValueType::Boolean,
            .Serializes = false,
        } },
    };

    return props;
}

void EcAnimation::SetAnimation(const std::string& Asset)
{
    Animation = Asset;

    if (Asset.size() == 0)
    {
        AnimationId = UINT32_MAX;
        return;
    }

    const std::string path = FileRW::ResolvePathNormalized(Asset);

    AnimatorComponentManager* acm = (AnimatorComponentManager*)AnimatorComponentManager::Get();
    if (const auto& it = acm->RegisteredAnimations.find(path); it != acm->RegisteredAnimations.end())
    {
        AnimationId = it->second;
        return;
    }

    assert(acm->Animations.size() < (size_t)UINT32_MAX);
    uint32_t id = (uint32_t)acm->Animations.size();
    AnimationData& data = acm->Animations.emplace_back();
    acm->RegisteredAnimations[path] = id;
    data.File = path;

    bool found = false;
    std::string animFileContents = FileRW::ReadFile(path, &found);

    if (!found)
    {
        Log.ErrorF("Cannot find animation file '{}'", path);
        return;
    }

    constexpr std::string_view Magic = "PHOENIXF/ANIM\n\0";

    if (animFileContents.find(Magic) != 0)
    {
        Log.ErrorF("Invalid or corrupt animation file: Invalid magic");
        return;
    }

    std::string_view animationData = std::string_view(animFileContents.begin() + Magic.size(), animFileContents.end());
    size_t cursor = 0;
    bool eof = false;

    uint32_t flags = ReadU32(animationData, &cursor, &eof);
    if (eof)
        RAISE_RT("Reached end of animation file trying to read flags");

    if (flags < 1)
        RAISE_RT("Animation {} is too old, please re-import it", path);

    data.Length = ReadF32(animationData, &cursor, &eof);
    if (eof)
        RAISE_RT("Reached end of animation file trying to read animation length");

    uint32_t keyframeCount = ReadU32(animationData, &cursor, &eof);
    if (eof)
        RAISE_RT("Reached end of animation file trying to read keyframe count");

    data.Keyframes.reserve(keyframeCount);
    uint16_t boneCount = ReadU16(animationData, &cursor, &eof);
    if (eof)
        RAISE_RT("Reached end of animation file trying to read bone count");

    data.Bones.reserve(boneCount);

    for (uint16_t bi = 0; bi < boneCount; bi++)
    {
        uint8_t nameLength = ReadU8(animationData, &cursor, &eof);
        if (eof)
            RAISE_RT("Reached end of animation file trying to read bone names");

        data.Bones.emplace_back(animationData.data() + cursor, nameLength);
        cursor += nameLength;
    }

    for (uint32_t keyframeIndex = 0; keyframeIndex < keyframeCount; keyframeIndex++)
    {
        AnimationData::Keyframe& kf = data.Keyframes.emplace_back();
        kf.Time = ReadF32(animationData, &cursor, &eof);

        uint8_t poseCount = ReadU8(animationData, &cursor, &eof);
        kf.Poses.reserve(poseCount);

        for (uint8_t poseIndex = 0; poseIndex < poseCount; poseIndex++)
        {
            AnimationData::Pose& pose = kf.Poses.emplace_back();
            pose.BoneId = ReadU16(animationData, &cursor, &eof);

            float tx = ReadF32(animationData, &cursor, &eof);
            float ty = ReadF32(animationData, &cursor, &eof);
            float tz = ReadF32(animationData, &cursor, &eof);

            float rx = ReadF32(animationData, &cursor, &eof);
            float ry = ReadF32(animationData, &cursor, &eof);
            float rz = ReadF32(animationData, &cursor, &eof);
            float rw = ReadF32(animationData, &cursor, &eof);

            float sx = ReadF32(animationData, &cursor, &eof);
            float sy = ReadF32(animationData, &cursor, &eof);
            float sz = ReadF32(animationData, &cursor, &eof);

            pose.Translation = { tx, ty, tz };
            pose.Rotation = { rw, rx, ry, rz };
            pose.Scale = { sx, sy, sz };

            if (eof)
                RAISE_RT("Reached end of animation file reading pose {} of {} in {}", poseIndex + 1, poseCount, path);
        }

        if (eof)
            RAISE_RT("Reached end of animation file reading keyframe {} of {} in {}", keyframeIndex + 1, keyframeCount, path);
    }

    AnimationId = id;
}

uint32_t AnimatorComponentManager::CreateComponent(GameObject* Object)
{
    uint32_t id = ComponentManager<EcAnimator>::CreateComponent(Object);
    Components[id].Object = Object;

    return id;
}

void AnimatorComponentManager::DeleteComponent(uint32_t Id)
{
    Components[Id].Joints.clear();
    ComponentManager<EcAnimator>::DeleteComponent(Id);
}

const Reflection::StaticPropertyMap& AnimatorComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY_SIMPLE(EcAnimator, Animating, Boolean),
    };

    return props;
}

const Reflection::StaticMethodMap& AnimatorComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap methods = {
        /*
        { "LoadAnimation", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::GameObject }), // `Animation`
            {},
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                EcAnimator* ea = static_cast<EcAnimator*>(p);
                GameObject* anim = GameObjectManager::Get()->FromGenericValue(inputs[0]);

                if (EcAnimation* animation = anim->FindComponent<EcAnimation>())
                {
                    if (animation->AnimationId == UINT32_MAX)
                        animation->SetAnimation(animation->Animation);

                    ea->LoadAnimation(anim, animation->AnimationId);
                    return {};
                }
                else
                    RAISE_RT("GameObject must have an `AnimationAsset` component");
            }
        } },
        */
    };

    return methods;
}

void EcAnimator::LoadAnimation(ObjectHandle stateObj, uint32_t Id)
{
    if (Id == UINT32_MAX)
        RAISE_RT("Animation is not valid, failed to load or path is blank");

    for (const ObjectHandle& loaded : Animations)
    {
        if (EcAnimation* eas = loaded->FindComponent<EcAnimation>(); eas && eas->AnimationId == Id)
            return;
    }

    stateObj->FindComponent<EcAnimation>()->AnimationId = Id;
    Animations.push_back(stateObj);
    BuildRig();
}

void EcAnimator::BuildRig()
{
    ZoneScoped;
    Joints.clear();

    Object->ForEachDescendant([this](const ObjectHandle& desc)
    {
        if (desc->FindComponent<EcTransform>() || desc->FindComponent<EcBone>())
            Joints[desc->Name].push_back(desc);

        return true;
    });
}

void EcAnimator::Step(double DeltaTime)
{
    ZoneScoped;
    AnimatorComponentManager* acm = (AnimatorComponentManager*)AnimatorComponentManager::Get();

    for (const ObjectHandle& animObj : Animations)
    {
        EcAnimation* animationState = animObj->FindComponent<EcAnimation>();
        if (!animationState || !animationState->Playing)
            continue;

        const AnimationData& animation = acm->Animations.at(animationState->AnimationId);

        animationState->Time += (float)DeltaTime;
        if (animationState->Time > animation.Length)
        {
            animationState->Time = 0.f;

            if (!animationState->Looped)
                animationState->Playing = false;
        }

        const AnimationData::Keyframe* prevKeyframe = nullptr;
        const AnimationData::Keyframe* nextKeyframe = nullptr;

        for (const AnimationData::Keyframe& kf : animation.Keyframes)
        {
            if (kf.Time <= animationState->Time)
            {
                if (!prevKeyframe || kf.Time > prevKeyframe->Time)
                    prevKeyframe = &kf;
            }

            if (kf.Time > animationState->Time)
            {
                if (!nextKeyframe || kf.Time < nextKeyframe->Time)
                    nextKeyframe = &kf;
            }
        }

        if (!prevKeyframe || !nextKeyframe)
            continue;

        float alpha = (animationState->Time - prevKeyframe->Time) / (nextKeyframe->Time - prevKeyframe->Time);

        std::unordered_set<EcMesh*> meshes;
        for (const AnimationData::Pose& pose : prevKeyframe->Poses)
        {
            const std::string& targetName = animation.Bones[pose.BoneId];
            const AnimationData::Pose* next = nullptr;

            for (const AnimationData::Pose& maybe : nextKeyframe->Poses)
            {
                if (maybe.BoneId == pose.BoneId)
                {
                    next = &maybe;
                    break;
                }
            }

            if (!next)
                continue;

            glm::vec3 trans = glm::mix(pose.Translation, next->Translation, alpha);
            glm::quat rot = glm::slerp(pose.Rotation, next->Rotation, alpha);
            glm::vec3 scale = glm::mix(pose.Scale, next->Scale, alpha);
            glm::mat4 transs = glm::translate(glm::mat4(1.f), trans) * glm::mat4_cast(rot) * glm::scale(glm::mat4(1.f), scale);

            for (auto& [ jointName, affected ] : Joints)
            {
                if (jointName == targetName)
                {
                    for (ObjectHandle& joint : affected)
                    {
                        if (EcBone* cb = joint->FindComponent<EcBone>())
                        {
                            cb->SetTransform(transs);

                            EcMesh* mesh = (EcMesh*)GetComponentManagerByComponentType(EntityComponent::Mesh)->GetComponent(cb->TargetMesh);
                            meshes.insert(mesh);
                        }
                        else if (EcTransform* ct = joint->FindComponent<EcTransform>())
                        {
                            ct->LocalTransform = transs;
                            ct->RecomputeTransformTree();
                        }
                    }
                }
            }
        }

        for (const auto& it : meshes)
            it->RecomputeBoneMatrices();
    }
}
