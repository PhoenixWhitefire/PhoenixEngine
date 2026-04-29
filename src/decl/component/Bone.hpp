#pragma once

#include <cstdint>

#include "datatype/GameObject.hpp"

struct EcBone : public Component<EntityComponent::Bone>
{
	void SetTransform(const glm::mat4&);

	glm::mat4 Transform = glm::mat4(1.f);
	uint32_t TargetMesh = UINT32_MAX;
	uint8_t SkeletalBoneId = UINT8_MAX;

	ObjectRef Object;
	bool Valid = true;
};

class BoneComponentManager : public ComponentManager<EcBone>
{
public:
	uint32_t CreateComponent(GameObject* Object) override;
	const Reflection::StaticPropertyMap& GetProperties() override;
};
