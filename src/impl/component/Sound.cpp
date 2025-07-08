#include <tracy/public/tracy/Tracy.hpp>
#include <Vendor/FMOD/api/core/inc/fmod.hpp>
#include <Vendor/FMOD/api/core/inc/fmod_errors.h>

#include "component/Sound.hpp"
#include "datatype/GameObject.hpp"
#include "GlobalJsonConfig.hpp"
#include "ThreadManager.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

#define SCOPED_LOCK(mtx) std::unique_lock<std::mutex> __lock_##mtx(mtx)
#define FMOD_CALL(expr, op) { \
	if (FMOD_RESULT result = (expr); result != FMOD_OK) { \
		std::string errstr = std::format("FMOD " op " failed: code {} - '{}'", (int)result, FMOD_ErrorString(result)); \
		Log::Error(errstr); \
		throw(std::runtime_error(errstr)); \
	} \
} \

static std::unordered_map<std::string, std::pair<FMOD::Sound*, std::vector<uint32_t>>> AudioAssets{};
static std::unordered_map<FMOD::Sound*, std::string> SoundToSoundFile{};
static std::unordered_map<void*, uint32_t> ChannelToComponent{};
static std::mutex AudioAssetsMutex;
static std::mutex ComponentsMutex;
//static std::vector<std::promise<bool>> AudioStreamPromises;

class SoundManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
		if (!m_DidInit)
			Initialize();

		SCOPED_LOCK(ComponentsMutex);

        m_Components.emplace_back();
		m_Components.back().Object = Object;
		m_Components.back().EcId = static_cast<uint32_t>(m_Components.size() - 1);
		//AudioStreamPromises.emplace_back();

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcSound& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

    virtual void* GetComponent(uint32_t Id) override
    {
        return &m_Components[Id];
    }

    virtual void DeleteComponent(uint32_t Id) override
    {
		SCOPED_LOCK(ComponentsMutex);

        // TODO id reuse with handles that have a counter per re-use to reduce memory growth
		EcSound& sound = m_Components[Id];

		if (FMOD::Channel* chan = (FMOD::Channel*)sound.m_Channel)
			chan->stop();

		sound.Object.Invalidate();
    }

    virtual const Reflection::PropertyMap& GetProperties() override
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
					if (!sound->Object->Enabled && playing.AsBoolean())
						RAISE_RT("Tried to play Sound while Object was disabled");

					sound->m_PlayRequested = playing.AsBoolean();

					if (FMOD::Channel* chan = (FMOD::Channel*)sound->m_Channel)
						chan->setPaused(!sound->m_PlayRequested);
				}
			),

			EC_PROP(
				"Position",
				Double,
				[](void* p)
				{
					EcSound* sound = static_cast<EcSound*>(p);

					if (FMOD::Channel* chan = (FMOD::Channel*)sound->m_Channel)
					{
						uint32_t pos = 0;
						FMOD_CALL(chan->getPosition(&pos, FMOD_TIMEUNIT_MS), "get channel time position");

						return (double)pos / 1000;
					}
					else
						return 0.0;
				},
				[](void* p, const Reflection::GenericValue& gv)
				{
					EcSound* sound = static_cast<EcSound*>(p);
					double t = gv.AsDouble();

					if (t < 0.f)
						RAISE_RT("Position cannot be negative");

					if (FMOD::Channel* chan = (FMOD::Channel*)sound->m_Channel)
					{
						uint32_t pos = static_cast<uint32_t>(t * 1000);
						FMOD_CALL(chan->setPosition(pos, FMOD_TIMEUNIT_MS), "set channel time position");
					}
					else
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

    virtual const Reflection::FunctionMap& GetFunctions() override
    {
        static const Reflection::FunctionMap funcs = {};
        return funcs;
    }
	
	SoundManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::Sound] = this;
    }

	void Initialize()
	{
		ZoneScoped;

		FMOD_CALL(FMOD::System_Create(&FmodSystem), "System creation");
		FMOD_CALL(FmodSystem->init(32, FMOD_INIT_NORMAL, 0), "System initialization");

		m_DidInit = true;
	}

	void Update()
	{
		ZoneScoped;

		FMOD_CALL(FmodSystem->update(), "System update");
		LastTick = GetRunningTime();
	}

	virtual void Shutdown() override
	{
		FMOD_CALL(FmodSystem->release(), "System shutdown");

		for (size_t i = 0; i < m_Components.size(); i++)
            DeleteComponent(i);
		
		AudioAssets.clear();
	}

	FMOD::System* FmodSystem = nullptr;
	double LastTick = 0.f;

private:
    std::vector<EcSound> m_Components;
	bool m_DidInit = false;
};

static inline SoundManager Instance{};

static FMOD_RESULT F_CALL fmodNonBlockingCallback(FMOD_SOUND* Sound, FMOD_RESULT Result)
{
	FMOD::Sound* sound = (FMOD::Sound*)Sound;
	const auto& it = AudioAssets.find(SoundToSoundFile[sound]);

	if (Result != FMOD_OK)
	{
		SCOPED_LOCK(AudioAssetsMutex);
		SCOPED_LOCK(ComponentsMutex);

		Log::Warning(std::format(
			"FMOD sound failed to load: code {} - '{}'",
			(int)Result, FMOD_ErrorString(Result))
		);

		for (uint32_t ecId : it->second.second)
		{
			EcSound* ecSound = (EcSound*)Instance.GetComponent(ecId);
			ecSound->FinishedLoading = true;
			ecSound->LoadSucceeded = false;
		}
	}
	else
	{
		SCOPED_LOCK(AudioAssetsMutex);
		SCOPED_LOCK(ComponentsMutex);

		for (uint32_t ecId : it->second.second)
		{
			EcSound* ecSound = (EcSound*)Instance.GetComponent(ecId);
			ecSound->FinishedLoading = true;
			ecSound->LoadSucceeded = true;
		}
	}

	return FMOD_OK;
}

static FMOD_RESULT F_CALL channelCtlCallback(
	FMOD_CHANNELCONTROL* Channel,
	FMOD_CHANNELCONTROL_TYPE ChannelType,
	FMOD_CHANNELCONTROL_CALLBACK_TYPE CallbackType,
	void* CommandData1,
	void* CommandData2
)
{
	if (CallbackType == FMOD_CHANNELCONTROL_CALLBACK_END)
	{
		uint32_t ecId = ChannelToComponent[(void*)Channel];
		if (EcSound* sound = (EcSound*)Instance.GetComponent(ecId))
		{
			if (!sound->Looped)
			{
				((FMOD::Channel*)Channel)->stop();
				sound->m_PlayRequested = false;
				sound->m_Channel = nullptr;
			}
		}
	}

	return FMOD_OK;
}

void EcSound::Update(double)
{
	if (GetRunningTime() != Instance.LastTick)
		Instance.Update();

	if (FinishedLoading && m_Channel)
		return;

	if (!LoadSucceeded)
		return;

	if (!m_Channel && m_PlayRequested)
	{
		FMOD::Sound* sound = AudioAssets[SoundFile].first;
		assert(sound);

		FMOD::Channel*& chan = (FMOD::Channel*&)m_Channel;

		FMOD_CALL(Instance.FmodSystem->playSound(
			sound,
			nullptr,
			!m_PlayRequested,
			&chan
		), "play nonblocking sound");
		ChannelToComponent[m_Channel] = EcId;

		FMOD_CALL(chan->setCallback(channelCtlCallback), "set channel control callback");
		FMOD_CALL(chan->setMode(Looped ? FMOD_LOOP_NORMAL : FMOD_DEFAULT), "set channel mode [1]");
	}
	else if (FMOD::Channel* chan = (FMOD::Channel*)m_Channel; chan && m_PlayRequested)
	{
		bool paused = false;
		FMOD_CALL(chan->getPaused(&paused), "get is paused");

		if (paused == m_PlayRequested)
			FMOD_CALL(chan->setPaused(!m_PlayRequested), "set paused");
		
		FMOD_MODE mode;
		FMOD_CALL(chan->getMode(&mode), "get channel mode [2]");

		if (bool chlooped = mode & FMOD_LOOP_NORMAL; chlooped != Looped)
			FMOD_CALL(chan->setMode(Looped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF), "sync loop state");
	}
}

void EcSound::Reload()
{
	if (!FinishedLoading)
		RAISE_RT("Cannot `::Reload` a Sound while it is still loading");

	if (const auto& it = AudioAssets.find(SoundFile); it == AudioAssets.end())
	{
		std::string ResDir = EngineJsonConfig.value("ResourcesDirectory", "resources/");

		FinishedLoading = false;
		LoadSucceeded = false;
		m_Channel = nullptr;

		FMOD::Sound* sound;
		FMOD_CREATESOUNDEXINFO exinfo{};
		exinfo.nonblockcallback = fmodNonBlockingCallback;
		exinfo.cbsize = sizeof(exinfo);

		FMOD_CALL(
			Instance.FmodSystem->createSound((ResDir + SoundFile).c_str(), FMOD_NONBLOCKING, &exinfo, &sound),
			"create sound"
		);

		SCOPED_LOCK(AudioAssetsMutex);

		std::pair<FMOD::Sound*, std::vector<uint32_t>>& pair = AudioAssets[SoundFile];
		pair.first = sound;
		pair.second.push_back(EcId);
		SoundToSoundFile[sound] = SoundFile;
	}
	else
	{
		FMOD::Channel*& chan = (FMOD::Channel*&)m_Channel;

		FMOD_CALL(Instance.FmodSystem->playSound(
			it->second.first,
			nullptr,
			!m_PlayRequested,
			&chan
		), "play sound");
		ChannelToComponent[m_Channel] = EcId;

		FMOD_CALL(chan->setCallback(channelCtlCallback), "set channel control callback");
		FMOD_CALL(chan->setMode(Looped ? FMOD_LOOP_NORMAL : FMOD_DEFAULT), "set channel mode");
	}
}
