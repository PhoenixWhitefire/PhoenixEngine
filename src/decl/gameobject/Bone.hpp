#pragma once

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
