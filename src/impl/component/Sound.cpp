#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include "component/Sound.hpp"
#include "datatype/GameObject.hpp"
#include "GlobalJsonConfig.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

struct AudioAsset
{
	SDL_AudioSpec Spec{};
	uint8_t* Data{};
	uint32_t DataSize{};
};

static std::unordered_map<std::string, AudioAsset> AudioAssets{};

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

static void streamCallback(void* UserData, SDL_AudioStream* Stream, int AdditionalAmount, int /*TotalAmount*/)
{
	EcSound* sound = static_cast<EcSound*>(UserData);
	const AudioAsset& audio = AudioAssets[sound->SoundFile];

	if (sound->NextRequestedPosition != -1.f)
	{
		float target = sound->NextRequestedPosition;
		sound->NextRequestedPosition = -1.f;

		sound->BytePosition = static_cast<uint32_t>(audio.Spec.freq * target);
	}

	if (sound->BytePosition >= audio.DataSize)
	{
		if (sound->Looped)
			SDL_PutAudioStreamData(Stream, audio.Data, AdditionalAmount);
		else
			SDL_PauseAudioStreamDevice(Stream);

		sound->BytePosition = 0;
	}
	else
	{
		int len = std::min(AdditionalAmount, static_cast<int>(audio.DataSize - sound->BytePosition));

		SDL_PutAudioStreamData(Stream, audio.Data + sound->BytePosition, len);
	}

	sound->Position = (float)sound->BytePosition / (float)audio.Spec.freq;
	sound->BytePosition += AdditionalAmount;
}

class SoundManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) final
    {
        m_Components.emplace_back();
		m_Components.back().Object = Object;

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() final
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (const EcSound& t : m_Components)
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

		SDL_DestroyAudioStream(static_cast<SDL_AudioStream*>(m_Components[Id].m_AudioStream));
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
					sound->Reload();
				}
			),

			EC_PROP(
				"Playing",
				Boolean,
				[](void* p)
				{
					EcSound* sound = static_cast<EcSound*>(p);
		
					if (sound->m_AudioStream)
						return !SDL_AudioStreamDevicePaused((SDL_AudioStream*)sound->m_AudioStream);
					else
						return false;
				},
				[](void* p, const Reflection::GenericValue& playing)
				{
					EcSound* sound = static_cast<EcSound*>(p);
					SDL_AudioStream* stream = (SDL_AudioStream*)sound->m_AudioStream;
		
					if (stream)
						if (playing.AsBoolean())
						{
							if (!sound->Object->Enabled)
								throw("Tried to play sound while Object was disabled, Object: " + sound->Object->GetFullName());

							const AudioAsset& audio = AudioAssets[sound->SoundFile];
		
							if (sound->BytePosition >= audio.DataSize || sound->BytePosition == 0)
							{
								sound->BytePosition = 1;
								SDL_PutAudioStreamData(stream, audio.Data, audio.DataSize);
							}
							
							SDL_ResumeAudioStreamDevice(stream);
						}
						else
						{
							SDL_PauseAudioStreamDevice(stream);
							SDL_FlushAudioStream(stream);
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
				}
			),

			EC_PROP_SIMPLE(EcSound, Looped, Boolean),

			EC_PROP("Length", Double, EC_GET_SIMPLE(EcSound, Length), nullptr)
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
			throw("Unabled to init SDL Audio subsystem: " + std::string(SDL_GetError()));

        GameObject::s_ComponentManagers[(size_t)EntityComponent::Sound] = this;
    }

private:
    std::vector<EcSound> m_Components;
};

static inline SoundManager Instance{};

void EcSound::Update(double)
{
	SDL_AudioStream* stream = (SDL_AudioStream*)m_AudioStream;

	if (!stream)
		return;

	if (!Object->Enabled && !SDL_AudioStreamDevicePaused(stream))
		if (!SDL_PauseAudioStreamDevice(stream))
			throw("Failed to pause audio stream (in ::Update): " + std::string(SDL_GetError()));
}

void EcSound::Reload()
{
	if (m_AudioStream)
		SDL_DestroyAudioStream((SDL_AudioStream*)m_AudioStream);

	m_AudioStream = nullptr;

	static std::string ResDir = EngineJsonConfig.value("ResourcesDirectory", "resources/");

	AudioAsset audio{};

	if (const auto& it = AudioAssets.find(SoundFile); it == AudioAssets.end())
	{
		/*
		SDL_AudioSpec fileSpec{};
		uint8_t* fileData{};
		uint32_t fileDataLen{};
		*/

		bool success = SDL_LoadWAV(
			(ResDir + SoundFile).c_str(),
			&audio.Spec,
			&audio.Data,
			&audio.DataSize
		);

		if (!success)
			throw("Failed to load sound file: " + std::string(SDL_GetError()));

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

		AudioAssets[SoundFile] = audio;
	}
	else
		audio = it->second;

	Length = (float)audio.DataSize / (float)audio.Spec.freq;

	SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		&audio.Spec,
		NULL,
		nullptr
	);
	m_AudioStream = stream;

	if (!stream)
		throw("Failed to create audio stream: " + std::string(SDL_GetError()));

	SDL_SetAudioStreamGetCallback(stream, streamCallback, this);

	SDL_PauseAudioStreamDevice(stream);
	BytePosition = 1;
}
