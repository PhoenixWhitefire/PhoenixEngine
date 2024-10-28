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

	PHX_GAMEOBJECT_API_REFLECTION;

private:
	static void s_DeclareReflections();
	static inline Api s_Api{};
};
