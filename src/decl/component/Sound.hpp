// A sound
// 26/10/2024

#pragma once

#include <string>
#include <future>
#include <cfloat>
#include <miniaudio/miniaudio.h>
#include <glm/mat4x4.hpp>

#include "datatype/GameObject.hpp"
#include "datatype/ComponentBase.hpp"

struct EcSound : public Component<EntityComponent::Sound>
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

	float NextRequestedPosition = -1.f;

	ObjectRef Object;
	uint32_t EcId = PHX_GAMEOBJECT_NULL_ID;

	std::vector<Reflection::EventCallback> OnLoadedCallbacks;
	ma_sound* SoundInstance;
	
	bool m_PlayRequested = false;
	bool Valid = true;
};

class SoundManager : public ComponentManager<EcSound>
{
public:
	static SoundManager& Get();

	uint32_t CreateComponent(GameObject*) override;
	void DeleteComponent(uint32_t) override;
	const Reflection::StaticPropertyMap& GetProperties() override;
	const Reflection::StaticMethodMap& GetMethods() override;
	const Reflection::StaticEventMap& GetEvents() override;

	void Initialize();
	void Update(const glm::mat4& CameraTransform);
	void Shutdown() override;

	ma_engine AudioEngine;
	double LastTick = 0.f;

private:
	bool m_DidInit = false;
};
