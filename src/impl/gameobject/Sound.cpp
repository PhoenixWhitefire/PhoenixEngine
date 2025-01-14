#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include "gameobject/Sound.hpp"
#include "GlobalJsonConfig.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Sound);

struct AudioAsset
{
	SDL_AudioSpec Spec;
	uint8_t* Data{};
	uint32_t DataSize{};
};

static bool s_DidInitReflection = false;
static std::unordered_map<std::string, AudioAsset> AudioAssets{};

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

			Log::Info("soundupdate");

			if (sound->AudioDeviceId != UINT32_MAX)
			{
				SDL_PauseAudioDevice(sound->AudioDeviceId);
				SDL_CloseAudioDevice(sound->AudioDeviceId);
			}

			sound->SoundFile = newFile;

			if (newFile == "")
				return;

			bool fileFound = true;
			FileRW::ReadFile(newFile, &fileFound);

			if (!fileFound)
			{
				Log::Error(std::vformat(
					"Cannot find Sound File '{}'",
					std::make_format_args(newFile)
				));

				return;
			}

			AudioAsset* asset = nullptr;

			if (AudioAssets.find(sound->SoundFile) == AudioAssets.end())
			{
				AudioAssets.insert(std::pair(sound->SoundFile, AudioAsset()));
				asset = &AudioAssets[sound->SoundFile];

				SDL_LoadWAV(
					FileRW::GetAbsolutePath(newFile).c_str(),
					&asset->Spec,
					&asset->Data,
					&asset->DataSize
				);
			}
			else
				asset = &AudioAssets[sound->SoundFile];

			//SDL_OpenAudio(&asset.Spec, NULL);

			//SDL_AudioSpec obtained{};

			//sound->AudioDeviceId = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &asset->Spec, NULL, NULL);

			//if (sound->AudioDeviceId == 0)
			//	throw("Could not `SDL_OpenAudioDevice`: " + std::string(SDL_GetError()));

			/*
			//SDL_QueueAudio(1, asset.Data, asset.DataSize);
			int qResult = SDL_QueueAudio(sound->AudioDeviceId, asset->Data, asset->DataSize);

			if (qResult < 0)
				throw("Could not `SDL_QueueAudio`: " + std::string(SDL_GetError()));

			SDL_PauseAudioDevice(sound->AudioDeviceId, 0);
			//SDL_PauseAudio(0);

			//__debugbreak();
			*/
		}
	);
}

Object_Sound::Object_Sound()
{
	this->Name = "Sound";
	this->ClassName = "Sound";

	s_DeclareReflections();
	ApiPointer = &s_Api;
}
