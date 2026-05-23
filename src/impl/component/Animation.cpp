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

const Reflection::StaticPropertyMap& AnimationAssetComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY(
            "Animation",
            String,
            REFLECTION_PROPERTY_GET_SIMPLE(EcAnimationAsset, Animation),
            [](void* p, const Reflection::GenericValue& gv)
            {

                static_cast<EcAnimationAsset*>(p)->SetAnimation(gv.AsString());
            }
        ),
    };

    return props;
}

void EcAnimationAsset::SetAnimation(const std::string& Asset)
{
    Animation = Asset;

    if (Asset.size() == 0)
    {
        AssetId = UINT32_MAX;
        return;
    }

    std::string path = FileRW::ResolvePathNormalized(Asset);

    AnimatorComponentManager* acm = (AnimatorComponentManager*)AnimatorComponentManager::Get();
    if (const auto& it = acm->RegisteredAnimations.find(path); it != acm->RegisteredAnimations.end())
    {
        AssetId = it->second;
        return;
    }

    uint32_t id = acm->Animations.size();
    AnimationData& data = acm->Animations.emplace_back();
    acm->RegisteredAnimations[path] = id;
    data.Path = path;

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
    cursor += 4; // flags

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

            float a1b1 = ReadF32(animationData, &cursor, &eof);
			float a1b2 = ReadF32(animationData, &cursor, &eof);
			float a1b3 = ReadF32(animationData, &cursor, &eof);
			float a1b4 = ReadF32(animationData, &cursor, &eof);

			float a2b1 = ReadF32(animationData, &cursor, &eof);
			float a2b2 = ReadF32(animationData, &cursor, &eof);
			float a2b3 = ReadF32(animationData, &cursor, &eof);
			float a2b4 = ReadF32(animationData, &cursor, &eof);

		    float a3b1 = ReadF32(animationData, &cursor, &eof);
		    float a3b2 = ReadF32(animationData, &cursor, &eof);
		    float a3b3 = ReadF32(animationData, &cursor, &eof);
		    float a3b4 = ReadF32(animationData, &cursor, &eof);

			float a4b1 = ReadF32(animationData, &cursor, &eof);
			float a4b2 = ReadF32(animationData, &cursor, &eof);
			float a4b3 = ReadF32(animationData, &cursor, &eof);
			float a4b4 = ReadF32(animationData, &cursor, &eof);

			pose.Transform = {
				a1b1, a1b2, a1b3, a1b4,
				a2b1, a2b2, a2b3, a2b4,
				a3b1, a3b2, a3b3, a3b4,
				a4b1, a4b2, a4b3, a4b4,
			};
        }

        if (eof)
            RAISE_RT("Reached end of animation file reading keyframe {} of {}", keyframeIndex, keyframeCount);
    }

    AssetId = id;
}

const Reflection::StaticPropertyMap& AnimationStateComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY_SIMPLE(EcAnimationState, Time, Double),
        REFLECTION_PROPERTY_SIMPLE(EcAnimationState, Weight, Double),
        REFLECTION_PROPERTY_SIMPLE(EcAnimationState, Playing, Boolean),
        REFLECTION_PROPERTY_SIMPLE(EcAnimationState, Looped, Boolean),
    };

    return props;
}

uint32_t AnimatorComponentManager::CreateComponent(GameObject* Object)
{
    uint32_t id = ComponentManager<EcAnimator>::CreateComponent(Object);
    m_Components[id].Object = Object;

    return id;
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
        { "LoadAnimation", Reflection::MethodDescriptor{
            { Reflection::ValueType::GameObject }, // `AnimationAsset`
            { Reflection::ValueType::GameObject }, // `AnimationState`
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                EcAnimator* ea = static_cast<EcAnimator*>(p);
                GameObject* animAsset = GameObjectManager::Get()->FromGenericValue(inputs[0]);

                if (EcAnimationAsset* eaa = animAsset->FindComponent<EcAnimationAsset>())
                {
                    if (eaa->AssetId == UINT32_MAX)
                        eaa->SetAnimation(eaa->Animation);
                    return { ea->LoadAnimation(eaa->AssetId)->ToGenericValue() };
                }
                else
                    RAISE_RT("GameObject must have an `AnimationAsset` component");
            }
        } },
    };

    return methods;
}

ObjectHandle EcAnimator::LoadAnimation(uint32_t Id)
{
    if (Id == UINT32_MAX)
        RAISE_RT("Animation is not valid, failed to load or path is blank");

    for (const ObjectHandle& loaded : Animations)
    {
        if (EcAnimationState* eas = loaded->FindComponent<EcAnimationState>(); eas && eas->AnimationAssetId == Id)
            return loaded;
    }

    ObjectHandle stateObj = GameObjectManager::s_Create(EntityComponent::AnimationState);
    EcAnimationState* eas = stateObj->FindComponent<EcAnimationState>();
    assert(eas);

    eas->AnimationAssetId = Id;
    stateObj->Serializes = false;
    Animations.push_back(stateObj);

    return stateObj;
}

void EcAnimator::Step(double DeltaTime)
{
    ZoneScoped;

    AnimatorComponentManager* acm = (AnimatorComponentManager*)AnimatorComponentManager::Get();

    for (const ObjectHandle& animObj : Animations)
    {
        EcAnimationState* animationState = animObj->FindComponent<EcAnimationState>();
        if (!animationState || !animationState->Playing)
            continue;

        const AnimationData& animation = acm->Animations.at(animationState->AnimationAssetId);

        animationState->Time += DeltaTime;
        if (animationState->Time > animation.Length)
        {
            animationState->Time = 0.f;

            if (!animationState->Looped)
                animationState->Playing = false;
        }

        const AnimationData::Keyframe* nearestKeyframe = nullptr;

        for (const AnimationData::Keyframe& kf : animation.Keyframes)
        {
            // no interpolation for now
            if (!nearestKeyframe || std::abs(kf.Time - animationState->Time) < std::abs(nearestKeyframe->Time - animationState->Time))
                nearestKeyframe = &kf;
        }

        if (!nearestKeyframe)
            continue;

        std::unordered_set<EcMesh*> meshes;
        for (const AnimationData::Pose& pose : nearestKeyframe->Poses)
        {
            const std::string& name = animation.Bones[pose.BoneId];

            Object->ForEachDescendant([&](const ObjectHandle& desc)
            {
                if (EcMesh* mesh = desc->FindComponent<EcMesh>())
                    meshes.insert(mesh);

                if (desc->Name == name)
                {
                    if (EcBone* cb = desc->FindComponent<EcBone>())
                    {
                        cb->SetTransform(pose.Transform);
                    }
                    else if (EcTransform* ct = desc->FindComponent<EcTransform>())
                    {
                        ct->LocalTransform = pose.Transform;
                        ct->RecomputeTransformTree();
                    }
                    else
                        return true;

                    //return false;
                }

                return true;
            });
        }

        for (const auto& it : meshes)
            it->RecomputeBoneMatrices();
    }
}
