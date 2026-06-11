#pragma once

#include <glm/mat4x4.hpp>
#include <string>

#include "datatype/ComponentBase.hpp"

struct EcAnimationAsset : public Component<EntityComponent::AnimationAsset>
{
    void SetAnimation(const std::string&);

    std::string Animation;
    uint32_t AssetId = UINT32_MAX;

    bool Valid = true;
};

class AnimationAssetComponentManager : public ComponentManager<EcAnimationAsset>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
};

struct EcAnimationState : public Component<EntityComponent::AnimationState>
{
    uint32_t AnimationAssetId = UINT32_MAX;
    float Time = 0.f;
    float Weight = 1.f;
    bool Playing = false;
    bool Looped = false;

    bool Valid = true;
};

struct AnimationData
{
    struct Pose
    {
        glm::mat4 Transform = glm::mat4(1.f);
        uint16_t BoneId = 0;
    };

    struct Keyframe
    {
        std::vector<Pose> Poses;
        float Time = 0.f;
    };

    std::string Path;
    std::vector<Keyframe> Keyframes;
    std::vector<std::string> Bones;
    float Length = 0.f;
};

class AnimationStateComponentManager : public ComponentManager<EcAnimationState>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
};

struct EcAnimator : public Component<EntityComponent::Animator>
{
    ObjectHandle LoadAnimation(uint32_t Id);
    void Step(double DeltaTime);

    // `AnimationState`s
    std::vector<ObjectHandle> Animations;

    bool Animating = true;
    bool Valid = true;
};

class AnimatorComponentManager : public ComponentManager<EcAnimator>
{
public:
    uint32_t CreateComponent(GameObject*) override;

    const Reflection::StaticPropertyMap& GetProperties() override;
    const Reflection::StaticMethodMap& GetMethods() override;

    std::vector<AnimationData> Animations;
    std::unordered_map<std::string, uint32_t> RegisteredAnimations;
};
