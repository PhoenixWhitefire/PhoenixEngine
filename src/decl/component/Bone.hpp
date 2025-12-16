#pragma once

#include <stdint.h>

#include "datatype/GameObject.hpp"

struct EcBone : public Component<EntityComponent::Bone>
{
	glm::mat4 Transform = glm::mat4(1.f);
	uint32_t TargetMesh = UINT32_MAX;
	uint8_t SkeletalBoneId = UINT8_MAX;

	ObjectRef Object;
	bool Valid = true;
};

/*

#include "gameobject/Attachment.hpp"

class Object_Bone : public Object_Attachment
{
public:
	Object_Bone();

	uint8_t SkeletalBoneId = UINT8_MAX;

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};
};
*/
