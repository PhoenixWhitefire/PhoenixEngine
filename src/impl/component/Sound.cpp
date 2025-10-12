#include <tracy/public/tracy/Tracy.hpp>
#include <miniaudio/miniaudio.h>

#include "component/Sound.hpp"
#include "component/Transform.hpp"
#include "Memory.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

uint32_t SoundManager::CreateComponent(GameObject* Object)
{
	if (!m_DidInit)
		Initialize();
	
    m_Components.emplace_back();
	m_Components.back().Object = Object;
	m_Components.back().EcId = static_cast<uint32_t>(m_Components.size() - 1);
	//AudioStreamPromises.emplace_back();

    return static_cast<uint32_t>(m_Components.size() - 1);
}

void SoundManager::DeleteComponent(uint32_t Id)
{
    // TODO id reuse with handles that have a counter per re-use to reduce memory growth
	EcSound& sound = m_Components[Id];
	if (sound.SoundInstance.pDataSource)
		ma_sound_uninit(&sound.SoundInstance);

	ComponentManager<EcSound>::DeleteComponent(Id);
}

const Reflection::StaticPropertyMap& SoundManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = 
    {
        EC_PROP(
			"SoundFile",
			String,
			EC_GET_SIMPLE(EcSound, SoundFile),
			[](void* p, const Reflection::GenericValue& gv)
			{
				EcSound* sound = static_cast<EcSound*>(p);
				std::string_view newFile = gv.AsStringView();

				if (newFile == sound->SoundFile)
					return;

				sound->SoundFile = newFile;
				sound->NextRequestedPosition = 0.f;
				sound->Reload();
			}
		),

		EC_PROP(
			"Playing",
			Boolean,
			[](void* p)
			-> Reflection::GenericValue
			{
				EcSound* sound = static_cast<EcSound*>(p);
				return sound->m_PlayRequested;
			},
			[](void* p, const Reflection::GenericValue& playing)
			{
				EcSound* sound = static_cast<EcSound*>(p);
				if (!sound->Object->Enabled && playing.AsBoolean())
					RAISE_RT("Tried to play Sound while Object was disabled");

				sound->m_PlayRequested = playing.AsBoolean();
			}
		),

		EC_PROP(
			"Position",
			Double,
			EC_GET_SIMPLE(EcSound, Position),
			[](void* p, const Reflection::GenericValue& gv)
			{
				EcSound* sound = static_cast<EcSound*>(p);
				double t = gv.AsDouble();

				if (t < 0.f)
					RAISE_RT("Position cannot be negative");

				sound->NextRequestedPosition = static_cast<float>(t);
			}
		),

		EC_PROP_SIMPLE(EcSound, Looped, Boolean),
		EC_PROP(
			"Volume",
			Double,
			EC_GET_SIMPLE(EcSound, Volume),
			[](void* p, const Reflection::GenericValue& gv)
			{
				float volume = static_cast<float>(gv.AsDouble());
				if (volume < 0.f)
					RAISE_RT("Volume cannot be negative");

				static_cast<EcSound*>(p)->Volume = volume;
			}
		),
		EC_PROP(
			"Speed",
			Double,
			EC_GET_SIMPLE(EcSound, Speed),
			[](void* p, const Reflection::GenericValue& gv)
			{
				float speed = static_cast<float>(gv.AsDouble());

				if (speed < 0.01f)
					RAISE_RT("Speed cannot be less than 0.01");
				if (speed > 100.f)
					RAISE_RT("Speed cannot be greater than 100");

				static_cast<EcSound*>(p)->Speed = speed;
			}
		),

		EC_PROP("Length", Double, EC_GET_SIMPLE(EcSound, Length), nullptr),

		EC_PROP("FinishedLoading", Boolean, EC_GET_SIMPLE(EcSound, FinishedLoading), nullptr),
		EC_PROP("LoadSucceeded", Boolean, EC_GET_SIMPLE(EcSound, LoadSucceeded), nullptr)
    };

    return props;
}

// stupid compiler false positive warnings
#if defined(__GNUG__) && (__GNUG__ == 14)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

const Reflection::StaticEventMap& SoundManager::GetEvents()
{
	static const Reflection::StaticEventMap events =
	{
		REFLECTION_EVENT(EcSound, OnLoaded, Reflection::ValueType::Boolean)
	};

	return events;
}

#if defined(__GNUG__) && (__GNUG__ == 14)
#pragma GCC diagnostic pop
#endif
	
void SoundManager::Initialize()
{
	ZoneScoped;

	ma_engine_config config{};
	config.allocationCallbacks.onMalloc = [](size_t Size, void*)
		{
			return Memory::Alloc(Size, MEMCAT(Sound));
		};
	config.allocationCallbacks.onRealloc = [](void* P, size_t Size, void*)
		{
			return Memory::ReAlloc(P, Size, MEMCAT(Sound));
		};
	config.allocationCallbacks.onFree = [](void* P, void*)
		{
			Memory::Free(P);
		};

	if (ma_result initResult = ma_engine_init(&config, &AudioEngine); initResult != MA_SUCCESS)
		RAISE_RTF("Audio Engine init failed, error code: {}", (int)initResult);

	m_DidInit = true;
}

void SoundManager::Update(const glm::mat4& CameraTransform)
{
	ZoneScoped;

	ma_engine_listener_set_position(&AudioEngine, 0, CameraTransform[3][0], CameraTransform[3][2], CameraTransform[3][2]);

	const glm::vec4& forward = CameraTransform[2];
	ma_engine_listener_set_direction(&AudioEngine, 0, forward[0], forward[1], forward[2]);

	LastTick = GetRunningTime();
}

void SoundManager::Shutdown()
{
	//AudioAssets.clear();

	ma_engine_uninit(&AudioEngine);
	ComponentManager<EcSound>::Shutdown();
}

static inline SoundManager Manager{};

SoundManager& SoundManager::Get()
{
	return Manager;
}

void EcSound::Reload()
{
	ZoneScoped;

	std::string filePath = FileRW::MakePathCwdRelative(SoundFile);

	if (SoundInstance.pDataSource)
		ma_sound_uninit(&SoundInstance);
	SoundInstance = ma_sound();

	FinishedLoading = false;
	LoadSucceeded = false;
	Length = 0.f;

	if (ma_result result = ma_sound_init_from_file(&Manager.AudioEngine, filePath.c_str(), 0, NULL, NULL, &SoundInstance);
		result != MA_SUCCESS
	)
	{
		FinishedLoading = true;
		Log::ErrorF("Failed to load sound file '{}' for '{}', error code: {}", filePath, Object->GetFullName(), (int)result);
		return;
	}

	FinishedLoading = true;

	if (ma_result result = ma_sound_get_length_in_seconds(&SoundInstance, &Length); result != MA_SUCCESS)
	{
		Log::ErrorF("Failed to get length of sound '{}', error code: {}", Object->GetFullName(), (int)result);
		return;
	}

	REFLECTION_SIGNAL(OnLoadedCallbacks, SoundFile);
	FinishedLoading = true;
	LoadSucceeded = true;
}

void EcSound::Update(double)
{
	ZoneScoped;

	if (!SoundInstance.pDataSource)
		return;

	bool playing = ma_sound_is_playing(&SoundInstance);

	if (!Object->Enabled && playing)
	{
		if (ma_result result = ma_sound_stop(&SoundInstance); result != MA_SUCCESS)
			Log::ErrorF("Failed to stop sound '{}' (disabled object), error code: {}", Object->GetFullName(), (int)result);
		return;
	}

	if (playing && !m_PlayRequested)
	{
		if (ma_result result = ma_sound_stop(&SoundInstance); result != MA_SUCCESS)
			Log::ErrorF("Failed to play sound '{}', error code: {}", Object->GetFullName(), (int)result);
	}
	else if (!playing && m_PlayRequested)
		if (ma_result result = ma_sound_start(&SoundInstance); result != MA_SUCCESS)
			Log::ErrorF("Failed to stop sound '{}', error code: {}", Object->GetFullName(), (int)result);

	ma_sound_set_looping(&SoundInstance, Looped); // TODO doesn't work
	if (!Looped && Length - Position < 0.05f)
		m_PlayRequested = false;

	ma_sound_set_volume(&SoundInstance, Volume);
	
	if (NextRequestedPosition >= 0.f)
	{
		if (ma_result result = ma_sound_seek_to_second(&SoundInstance, NextRequestedPosition); result != MA_SUCCESS)
			Log::ErrorF(
				"Failed to seek to position {} (in seconds) for sound '{}', error code: {}",
				NextRequestedPosition, Object->GetFullName(), (int)result
			);
		NextRequestedPosition = -1.f;
	}

	if (ma_result result = ma_sound_get_cursor_in_seconds(&SoundInstance, &Position); result != MA_SUCCESS)
		Log::ErrorF("Failed to get playback position of sound '{}', error code: {}", Object->GetFullName(), (int)result);

	if (EcTransform* trans = Object->FindComponent<EcTransform>())
		ma_sound_set_position(&SoundInstance, trans->Transform[3][0], trans->Transform[3][1], trans->Transform[3][2]);
}
