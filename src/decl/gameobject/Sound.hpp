// A sound
// 26/10/2024

#pragma once

#include <unordered_map>

#include "gameobject/Attachment.hpp"

class Object_Sound : public Object_Attachment
{
public:
	Object_Sound();
	~Object_Sound();

	void Update(double) override;

	std::string SoundFile{};
	float Length{};
	float Position = 0.f;
	float NextRequestedPosition = -1.f;
	bool Looped = false;

	uint32_t BytePosition = 0;

	REFLECTION_DECLAREAPI;

private:
	void* m_AudioStream = nullptr;

	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};
};
