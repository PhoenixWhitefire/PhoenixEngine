#pragma once

#include <stdint.h>

#include "datatype/GameObject.hpp"

struct EcBone
{
	glm::mat4 Transform{ 1.f };
	uint8_t SkeletalBoneId = UINT8_MAX;
	ObjectRef Object;
	bool Valid = true;

	static const EntityComponent Type = EntityComponent::Bone;
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
