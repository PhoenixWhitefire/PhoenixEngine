#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include "gameobject/Sound.hpp"
#include "GlobalJsonConfig.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Sound);

struct AudioAsset
{
	SDL_AudioSpec Spec{};
	uint8_t* Data{};
	uint32_t DataSize{};
};

static bool s_DidInitReflection = false;
static std::unordered_map<std::string, AudioAsset> AudioAssets{};

static void streamCallback(void* UserData, SDL_AudioStream* Stream, int AdditionalAmount, int /*TotalAmount*/)
{
	Object_Sound* sound = static_cast<Object_Sound*>(UserData);
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

void Object_Sound::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	if (!SDL_Init(SDL_INIT_AUDIO))
		throw("Unabled to init SDL Audio subsystem: " + std::string(SDL_GetError()));

	REFLECTION_INHERITAPI(Attachment);

	REFLECTION_DECLAREPROP(
		"SoundFile",
		String,
		[](Reflection::Reflectable* p)
		-> Reflection::GenericValue
		{
			return static_cast<Object_Sound*>(p)->SoundFile;
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			Object_Sound* sound = static_cast<Object_Sound*>(p);
			std::string newFile = gv.AsString();

			if (newFile == sound->SoundFile)
				return;

			sound->SoundFile = newFile;

			if (sound->m_AudioStream)
				SDL_DestroyAudioStream((SDL_AudioStream*)sound->m_AudioStream);

			sound->m_AudioStream = nullptr;

			static std::string ResDir = EngineJsonConfig.value("ResourcesDirectory", "resources/");

			AudioAsset audio{};

			if (const auto& it = AudioAssets.find(newFile); it == AudioAssets.end())
			{
				bool success = SDL_LoadWAV(
					(ResDir + newFile).c_str(),
					&audio.Spec,
					&audio.Data,
					&audio.DataSize
				);

				if (!success)
					throw("Failed to load sound file: " + std::string(SDL_GetError()));

				AudioAssets[newFile] = audio;
			}
			else
				audio = it->second;

			sound->Length = (float)audio.DataSize / (float)audio.Spec.freq;

			SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(
				SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
				&audio.Spec,
				NULL,
				nullptr
			);
			sound->m_AudioStream = stream;

			if (!stream)
				throw("Failed to create audio stream: " + std::string(SDL_GetError()));

			SDL_SetAudioStreamGetCallback(stream, streamCallback, sound);

			SDL_PauseAudioStreamDevice(stream);
			sound->BytePosition = 1;
		}
	);

	REFLECTION_DECLAREPROP(
		"Playing",
		Bool,
		[](Reflection::Reflectable* p)
		{
			Object_Sound* sound = static_cast<Object_Sound*>(p);

			if (sound->m_AudioStream)
				return !SDL_AudioStreamDevicePaused((SDL_AudioStream*)sound->m_AudioStream);
			else
				return false;
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& playing)
		{
			Object_Sound* sound = static_cast<Object_Sound*>(p);
			SDL_AudioStream* stream = (SDL_AudioStream*)sound->m_AudioStream;

			if (stream)
				if (playing.AsBool())
				{
					const AudioAsset& audio = AudioAssets[sound->SoundFile];

					if (sound->BytePosition >= audio.DataSize || sound->BytePosition == 0)
					{
						sound->BytePosition = 1;
						SDL_PutAudioStreamData(stream, audio.Data, audio.DataSize);
					}

					SDL_ResumeAudioStreamDevice(stream);
				}
				else
					SDL_PauseAudioStreamDevice(stream);
		}
	);

	REFLECTION_DECLAREPROP(
		"Position",
		Double,
		[](Reflection::Reflectable* p)
		{
			return static_cast<Object_Sound*>(p)->Position;
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			float pos = static_cast<float>(gv.AsDouble());
			if (pos < 0.f)
				throw("Position of Sound playback cannot be negative");

			Object_Sound* sound = static_cast<Object_Sound*>(p);

			sound->NextRequestedPosition = pos;
			sound->Position = pos;
		}
	);

	REFLECTION_DECLAREPROP_SIMPLE(Object_Sound, Looped, Bool);
	REFLECTION_DECLAREPROP_SIMPLE_READONLY(Object_Sound, Length, Double);
}

Object_Sound::Object_Sound()
{
	this->Name = "Sound";
	this->ClassName = "Sound";

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

Object_Sound::~Object_Sound()
{
	if (m_AudioStream)
		SDL_DestroyAudioStream((SDL_AudioStream*)m_AudioStream);
}

void Object_Sound::Update(double)
{
	SDL_AudioStream* stream = (SDL_AudioStream*)m_AudioStream;

	if (!stream)
		return;

	if (!Enabled && !SDL_AudioStreamDevicePaused(stream))
		if (!SDL_PauseAudioStreamDevice(stream))
			throw("Failed to pause audio stream: " + std::string(SDL_GetError()));
}
