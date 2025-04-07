// A sound
// 26/10/2024

#pragma once

#include <string>

#include "datatype/GameObject.hpp"

struct EcSound
{
	void Update(double);
	void Reload();

	std::string SoundFile;
	GameObjectRef Object;
	float Length;
	float Position;
	float NextRequestedPosition = -1.f;
	bool Looped = false;

	uint32_t BytePosition = 0;

	void* m_AudioStream = nullptr;
};
