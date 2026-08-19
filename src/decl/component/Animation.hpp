#pragma once

#include <glm/mat4x4.hpp>
#include <string>

#include "datatype/ComponentBase.hpp"

struct EcAnimation : public Component<EntityComponent::Animation>
{
    void SetAnimation(const std::string&);

    std::string Animation;
    uint32_t AnimationId = UINT32_MAX;
    float Time = 0.f;
    float Weight = 1.f;
    bool Playing = false;
    bool Looped = false;

    bool Valid = true;
};

struct AnimationData
{
    enum class ChannelType : uint8_t
    {
        Translation,
        Rotation,
        Scale,
    };

    struct Pose
    {
        glm::vec3 Translation = {};
        glm::quat Rotation = {};
        glm::vec3 Scale = {};
        uint16_t BoneId = 0;
    };

    struct Keyframe
    {
        std::vector<Pose> Poses;
        float Time = 0.f;
    };

    std::string File;
    std::vector<Keyframe> Keyframes;
    std::vector<std::string> Bones;
    float Length = 0.f;
};

class AnimationComponentManager : public ComponentManager<EcAnimation>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
};

struct EcAnimator : public Component<EntityComponent::Animator>
{
    void Step(double DeltaTime);

    void LoadAnimation(ObjectHandle stateObj, uint32_t Id);
    void BuildRig();

    std::vector<ObjectHandle> Animations;
    std::unordered_map<std::string, std::vector<ObjectHandle>> Joints;

    bool SkeletonStale = false;
    bool Animating = true;
    bool Valid = true;
};

class AnimatorComponentManager : public ComponentManager<EcAnimator>
{
public:
    uint32_t CreateComponent(GameObject*) override;
    void DeleteComponent(uint32_t) override;

    const Reflection::StaticPropertyMap& GetProperties() override;
    const Reflection::StaticMethodMap& GetMethods() override;

    std::vector<AnimationData> Animations;
    std::unordered_map<std::string, uint32_t> RegisteredAnimations;
};
