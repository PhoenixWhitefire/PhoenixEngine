#include <vector>
#include <Vendor/nljson.hpp>

#include "component/Animation.hpp"
#include "datatype/GameObject.hpp"
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
    acm->Animations.emplace_back();
    acm->RegisteredAnimations[path] = id;

    AnimationData& data = acm->Animations.back();
    data.Path = path;

    bool found = false;
    std::string animFileContents = FileRW::ReadFile(path, &found);

    if (!found)
    {
        Log.ErrorF("Cannot find animation file '{}'", path);
        return;
    }

    nlohmann::json json;
    try
    {
        json = nlohmann::json::parse(animFileContents);
    }
    catch(const nlohmann::json::parse_error& e)
    {
        Log.ErrorF("Cannot parse animation file '{}': {}", path, e.what());
        return;
    }

    // TODO
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
                    return { ea->LoadAnimation(eaa->AssetId)->ToGenericValue() };
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
    return stateObj;
}

void EcAnimator::Step(double /* DeltaTime */)
{
}
