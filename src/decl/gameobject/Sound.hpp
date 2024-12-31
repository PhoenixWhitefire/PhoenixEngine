// A sound
// 26/10/2024

#pragma once

#include <unordered_map>

#include "gameobject/Attachment.hpp"

class Object_Sound : public Object_Attachment
{
public:
	Object_Sound();

	std::string SoundFile{};
	float Length{};

	uint32_t AudioDeviceId = UINT32_MAX;

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};
