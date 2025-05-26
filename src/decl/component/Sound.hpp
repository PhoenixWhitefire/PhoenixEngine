// A sound
// 26/10/2024

#pragma once

#include <string>
#include <future>

#include "datatype/GameObject.hpp"

struct EcSound
{
	void Update(double);
	void Reload();

	std::string SoundFile;
	float Position = 0.f;
	float Length = 0.f;
	float Volume = 1.f;
	float NextRequestedPosition = 0.f;
	bool Looped = false;
	bool FinishedLoading = true;

	uint32_t BytePosition = 0;

	GameObjectRef Object;
	uint32_t EcId = PHX_GAMEOBJECT_NULL_ID;

	void* m_AudioStream = nullptr;
	bool m_PlayRequested = false;

	static const EntityComponent Type = EntityComponent::Sound;
};
