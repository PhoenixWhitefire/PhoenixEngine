// A sound
// 26/10/2024

#pragma once

#include <string>
#include <future>
#include <cfloat>

#include "datatype/GameObject.hpp"

struct EcSound
{
	void Update(double);
	void Reload();

	std::string SoundFile;
	float Position = 0.f;
	float Length = 0.f;
	float Volume = 1.f;
	float Speed = 1.f;
	bool Looped = false;
	bool FinishedLoading = true;
	bool LoadSucceeded = false;

	float NextRequestedPosition = 0.f;
	uint32_t BytePosition = 0;

	ObjectRef Object;
	uint32_t EcId = PHX_GAMEOBJECT_NULL_ID;

	std::vector<Reflection::EventCallback> OnLoadedCallbacks;
	
	void* m_Channel = nullptr;
	bool m_PlayRequested = false;
	float m_BaseFrequency = FLT_MAX;
	bool Valid = true;

	static const EntityComponent Type = EntityComponent::Sound;
};
