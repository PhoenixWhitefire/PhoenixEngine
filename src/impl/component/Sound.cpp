#include <tracy/public/tracy/Tracy.hpp>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include "component/Sound.hpp"
#include "datatype/GameObject.hpp"
#include "GlobalJsonConfig.hpp"
#include "ThreadManager.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

#define SCOPED_LOCK(mtx) std::unique_lock<std::mutex> __lock_##mtx(mtx)

struct AudioAsset
{
	SDL_AudioSpec Spec{};
	uint8_t* Data{};
	uint32_t DataSize{};
};

static std::unordered_map<std::string, AudioAsset> AudioAssets{};
static std::mutex AudioAssetsMutex;
static std::mutex ComponentsBufferReAllocMutex;
//static std::vector<std::promise<bool>> AudioStreamPromises;

// the spec of the audio device
// must be standardized to remove overhead of 
// creating unique audio devices
// 26/01/2025 nvm does fuck all
static const SDL_AudioSpec AudioSpec =
{
	SDL_AudioFormat::SDL_AUDIO_S16LE,
	2,
	44100
};

class SoundManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) final
    {
		ComponentsBufferReAllocMutex.lock();

        m_Components.emplace_back();
		m_Components.back().Object = Object;
		m_Components.back().EcId = static_cast<uint32_t>(m_Components.size() - 1);
		//AudioStreamPromises.emplace_back();

		ComponentsBufferReAllocMutex.unlock();

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() final
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcSound& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

    virtual void* GetComponent(uint32_t Id) final
    {
        return &m_Components[Id];
    }

    virtual void DeleteComponent(uint32_t Id) final
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth
		EcSound& sound = m_Components[Id];

		if (sound.m_AudioStream)
			SDL_DestroyAudioStream(static_cast<SDL_AudioStream*>(sound.m_AudioStream));

		sound.m_AudioStream = nullptr;

		sound.Object.Invalidate();
    }

    virtual const Reflection::PropertyMap& GetProperties() final
    {
        static const Reflection::PropertyMap props = 
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
				{
					EcSound* sound = static_cast<EcSound*>(p);
					return sound->m_PlayRequested;
				},
				[](void* p, const Reflection::GenericValue& playing)
				{
					EcSound* sound = static_cast<EcSound*>(p);
					SDL_AudioStream* stream = (SDL_AudioStream*)sound->m_AudioStream;

					bool wasPlayRequested = sound->m_PlayRequested;
					sound->m_PlayRequested = sound->Object->Enabled && playing.AsBoolean();

					if (playing.AsBoolean() && !sound->Object->Enabled)
						throw("Tried to play sound while Object was disabled");

					if (stream)
					{
						if (wasPlayRequested != sound->m_PlayRequested)
							assert(SDL_FlushAudioStream(stream));

						if (sound->m_PlayRequested)
						{
							const AudioAsset& audio = AudioAssets[sound->SoundFile];

							int len = std::min(200, static_cast<int>(audio.DataSize - sound->BytePosition));
							assert(SDL_PutAudioStreamData(stream, audio.Data + sound->BytePosition, len));
							sound->BytePosition += 200;
							
							assert(SDL_ResumeAudioStreamDevice(stream));
						}
						else
							assert(SDL_PauseAudioStreamDevice(stream));
					}
				}
			),

			EC_PROP(
				"Position",
				Double,
				[](void* p)
				{
					return static_cast<EcSound*>(p)->Position;
				},
				[](void* p, const Reflection::GenericValue& gv)
				{
					float pos = static_cast<float>(gv.AsDouble());
					if (pos < 0.f)
						throw("Position of Sound playback cannot be negative");
		
					EcSound* sound = static_cast<EcSound*>(p);
		
					sound->NextRequestedPosition = pos;
					sound->Position = pos;

					if (sound->m_AudioStream)
						assert(SDL_FlushAudioStream((SDL_AudioStream*)sound->m_AudioStream));
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
						throw("Volume cannot be negative");
					
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
						throw("Speed cannot be less than 0.01");
					if (speed > 100.f)
						throw("Speed cannot be greater than 100");

					static_cast<EcSound*>(p)->Speed = speed;
				}
			),

			EC_PROP("Length", Double, EC_GET_SIMPLE(EcSound, Length), nullptr),

			EC_PROP("FinishedLoading", Boolean, EC_GET_SIMPLE(EcSound, FinishedLoading), nullptr)
        };

        return props;
    }

    virtual const Reflection::FunctionMap& GetFunctions() final
    {
        static const Reflection::FunctionMap funcs = {};
        return funcs;
    }
	
	SoundManager()
    {
		if (!SDL_Init(SDL_INIT_AUDIO))
			throw("Unable to init SDL Audio subsystem: " + std::string(SDL_GetError()));

        GameObject::s_ComponentManagers[(size_t)EntityComponent::Sound] = this;
    }

	virtual void Shutdown() final
	{
		for (size_t i = 0; i < m_Components.size(); i++)
            DeleteComponent(i);

		for (auto& it : AudioAssets)
			free(it.second.Data); // get leaksan to shut up
		
		AudioAssets.clear();
	}

private:
    std::vector<EcSound> m_Components;
};

static inline SoundManager Instance{};

static void streamCallback(void* UserData, SDL_AudioStream* Stream, int AdditionalAmount, int /*TotalAmount*/)
{
	SCOPED_LOCK(ComponentsBufferReAllocMutex);

	EcSound* sound = static_cast<EcSound*>(Instance.GetComponent(*(uint32_t*)&UserData));
	const AudioAsset& audio = AudioAssets[sound->SoundFile];
	assert(SDL_SetAudioStreamFrequencyRatio(Stream, sound->Speed));
	assert(SDL_SetAudioStreamGain(Stream, sound->Volume));

	if (sound->NextRequestedPosition != -1.f)
	{
		float target = sound->NextRequestedPosition;
		sound->NextRequestedPosition = -1.f;

		sound->BytePosition = static_cast<uint32_t>(audio.Spec.freq * target);

		assert(SDL_FlushAudioStream(Stream));
	}

	if (sound->BytePosition >= audio.DataSize)
	{
		if (sound->Looped)
			assert(SDL_PutAudioStreamData(Stream, audio.Data, AdditionalAmount));
		else
		{
			assert(SDL_PauseAudioStreamDevice(Stream));
			sound->m_PlayRequested = false; // prevent it from resuming in `::Update`
		}

		sound->BytePosition = 0;
	}
	else
	{
		int len = std::min(AdditionalAmount, static_cast<int>(audio.DataSize - sound->BytePosition));

		assert(SDL_PutAudioStreamData(Stream, audio.Data + sound->BytePosition, len));
	}

	uint32_t sampleSize = SDL_AUDIO_BITSIZE(audio.Spec.format) / 8;
	uint32_t sampleCount = sound->BytePosition / sampleSize;
	
	assert(audio.Spec.channels > 0);
	uint32_t sampleLen = sampleCount / audio.Spec.channels;

	sound->Position = (float)sampleLen / (float)audio.Spec.freq;
	sound->BytePosition += AdditionalAmount;
}

void EcSound::Update(double)
{
	SDL_AudioStream* stream = (SDL_AudioStream*)m_AudioStream;

	if (!stream)
		return;
	
	if (Object->Enabled)
	{
		if (SDL_AudioStreamDevicePaused(stream) == m_PlayRequested)
		{
			if (m_PlayRequested)
			{
				if (!SDL_ResumeAudioStreamDevice(stream))
					throw("Failed to resume audio stream when it was requested: " + std::string(SDL_GetError()));
			}
			else
				if (!SDL_PauseAudioStreamDevice(stream))
					throw("Failed to pause audio stream when it was requested: " + std::string(SDL_GetError()));
		}
	}
	else
		if (!SDL_PauseAudioStreamDevice(stream))
			throw("Failed to pause audio stream of a disabled Sound: " + std::string(SDL_GetError()));
}

void EcSound::Reload()
{
	if (!FinishedLoading)
		throw("Cannot `::Reload` a Sound while it is still loading");

	if (m_AudioStream)
		SDL_DestroyAudioStream((SDL_AudioStream*)m_AudioStream);

	m_AudioStream = nullptr;

	std::string ResDir = EngineJsonConfig.value("ResourcesDirectory", "resources/");

	FinishedLoading = false;

	//std::promise<bool> begone;
	//AudioStreamPromises[EcId].swap(begone);

	uint32_t SoundEcId = EcId;
	std::string RequestedSoundFile = SoundFile;

	ThreadManager::Get()->Dispatch(
		[ResDir, SoundEcId, RequestedSoundFile]()
		{
			ZoneScopedN("AsyncSoundLoad");

			AudioAsset audio{};

			AudioAssetsMutex.lock();

			if (const auto& it = AudioAssets.find(RequestedSoundFile); it == AudioAssets.end())
			{
				/*
				SDL_AudioSpec fileSpec{};
				uint8_t* fileData{};
				uint32_t fileDataLen{};
				*/
			
				bool success = SDL_LoadWAV(
					(ResDir + RequestedSoundFile).c_str(),
					&audio.Spec,
					&audio.Data,
					&audio.DataSize
				);
			
				if (!success)
				{
					AudioAssetsMutex.unlock();

					Log::Error(std::format("Failed to load sound file '{}': {}", RequestedSoundFile, SDL_GetError()));

					SCOPED_LOCK(ComponentsBufferReAllocMutex);

					EcSound* sound = static_cast<EcSound*>(Instance.GetComponent(SoundEcId));
					sound->FinishedLoading = true;

					return;
				}
				/*
				// `SDL_ConvertAudioSamples` takes `int*` as the length
				// instead of `uint32_t*`?? why 26/01/2025
				int dataLen{};
			
				bool cvtSuccess = SDL_ConvertAudioSamples(
					&fileSpec,
					fileData,
					fileDataLen,
					&AudioSpec,
					&audio.Data,
					&dataLen
				);
			
				if (!cvtSuccess)
					throw("Failed to convert audio: " + std::string(SDL_GetError()));
			
				audio.DataSize = static_cast<uint32_t>(dataLen);
				*/
			
				AudioAssets[RequestedSoundFile] = audio;
			}
			else
				audio = it->second;
			
			AudioAssetsMutex.unlock();

			SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(
				SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
				&audio.Spec,
				NULL,
				nullptr
			);
		
			if (!stream)
			{
				Log::Error(std::format(
					"Failed to create audio stream for sound with file '{}': {}",
					RequestedSoundFile,
					SDL_GetError()
				));

				SCOPED_LOCK(ComponentsBufferReAllocMutex);

				EcSound* sound = static_cast<EcSound*>(Instance.GetComponent(SoundEcId));
				sound->FinishedLoading = true;

				return;
			}

			assert(SDL_SetAudioStreamGetCallback(stream, streamCallback, (void*)static_cast<uint64_t>(SoundEcId)));

			SCOPED_LOCK(ComponentsBufferReAllocMutex);

			EcSound* sound = static_cast<EcSound*>(Instance.GetComponent(SoundEcId));

			uint32_t sampleSize = SDL_AUDIO_BITSIZE(audio.Spec.format) / 8;
			uint32_t sampleCount = audio.DataSize / sampleSize;

			assert(audio.Spec.channels > 0);
			uint32_t sampleLen = sampleCount / audio.Spec.channels;

			sound->Length = (float)sampleLen / (float)audio.Spec.freq;
			sound->BytePosition = 1;
			sound->m_AudioStream = stream;
			sound->FinishedLoading = true;

			if (sound->m_PlayRequested)
				assert(SDL_ResumeAudioStreamDevice(stream));
			else
				assert(SDL_PauseAudioStreamDevice(stream));

			//AudioStreamPromises[SoundEcId].set_value(true);
		},
		false
	);
}
