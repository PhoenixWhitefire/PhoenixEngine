#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <luau/VM/include/lualib.h>
#include <tracy/Tracy.hpp>
#include <Luau/Compiler.h>
#include <format>
#include <stack>

#include "component/Script.hpp"
#include "script/ScriptEngine.hpp"
#include "datatype/Color.hpp"
#include "UserInput.hpp"
#include "Timing.hpp"
#include "FileRW.hpp"
#include "Memory.hpp"
#include "Log.hpp"

static lua_State* LVM = nullptr;

class ScriptManager : public ComponentManager<EcScript>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
		m_Components.back().Object = Object;
		m_Components.back().EcId = static_cast<uint32_t>(m_Components.size() - 1);

		return m_Components.back().EcId;
    }

	void UpdateComponent(uint32_t Id, double DeltaTime) override
	{
		m_Components[Id].Update(DeltaTime);
	}

    void DeleteComponent(uint32_t Id) override
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth
		if (lua_State** L = &m_Components[Id].m_L; *L)
		{
			lua_resetthread(*L);
			*L = nullptr;
		}

		ComponentManager<EcScript>::DeleteComponent(Id);
    }

    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
            { "SourceFile", {
				Reflection::ValueType::String,
				EC_GET_SIMPLE(EcScript, SourceFile),

				[](void* p, const Reflection::GenericValue& gv)
				{
					EcScript* s = static_cast<EcScript*>(p);
					s->LoadScript(gv.AsStringView());
				}
			} }
        };

        return props;
    }

    const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap funcs =
		{
			{ "Reload", {
				{},
				{ Reflection::ValueType::Boolean },
				[](void* p, const std::vector<Reflection::GenericValue>&)
				-> std::vector<Reflection::GenericValue>
				{
					EcScript* scr = static_cast<EcScript*>(p);
				
					bool reloadSuccess = scr->Reload();
					return { reloadSuccess };
				}
			} }
		};
        return funcs;
    }

	void Shutdown() override
    {
		ComponentManager<EcScript>::Shutdown();

		if (LVM)
			ScriptEngine::L::Close(LVM);
		LVM = nullptr;
    }
};

static inline ScriptManager ManagerInstance{};

static int shouldResume_Scheduled(
	const ScriptEngine::YieldedCoroutine& CorInfo,
	lua_State* L
)
{
	if (double curTime = GetRunningTime(); curTime >= CorInfo.RmSchedule.ResumeAt)
	{
		lua_pushnumber(L, curTime - CorInfo.RmSchedule.YieldedAt);

		return 1;
	}
	else
		return -1;
}

static int shouldResume_Future(const ScriptEngine::YieldedCoroutine& CorInfo, lua_State* L)
{
	const std::shared_future<std::vector<Reflection::GenericValue>>& future = CorInfo.RmFuture;

	if ( future.valid()
		 && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready
	)
	{
		std::vector<Reflection::GenericValue> returnVals = future.get();
		for (const Reflection::GenericValue& v : returnVals)
			ScriptEngine::L::PushGenericValue(L, v);

		return (int)returnVals.size();
	}
	else
		return -1;
}

static int shouldResume_Polled(const ScriptEngine::YieldedCoroutine& CorInfo, lua_State* L)
{
	return CorInfo.RmPoll(L);
}

typedef int(*ResumptionModeHandler)(const ScriptEngine::YieldedCoroutine&, lua_State*);

static const ResumptionModeHandler s_ResumptionModeHandlers[] =
{
	nullptr,

	shouldResume_Scheduled,
	shouldResume_Future,
	shouldResume_Polled
};

static void resumeYieldedCoroutines()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	size_t size = ScriptEngine::s_YieldedCoroutines.size();

	for (size_t i = 0; i < size; i++)
	{
		auto it = &ScriptEngine::s_YieldedCoroutines[i];

		lua_State* coroutine = it->Coroutine;
		int corRef = it->CoroutineReference;

		uint8_t resHandlerIndex = static_cast<uint8_t>(it->Mode);
		const ResumptionModeHandler handler = s_ResumptionModeHandlers[resHandlerIndex];
		assert(handler);

		int nretvals = handler(*it, coroutine);

		if (nretvals >= 0)
		{
			// make sure the script still exists
			// modules don't have a `script` global, but they run on their own coroutine independent from
			// where they are `require`'d from anyway
			if (it->ScriptId == PHX_GAMEOBJECT_NULL_ID || GameObject::GetObjectById(it->ScriptId))
			{
				int resumeStatus = lua_resume(coroutine, nullptr, nretvals);

				if (resumeStatus != LUA_OK && resumeStatus != LUA_YIELD)
				{
					Log::ErrorF(
						"Script resumption: {}",
						lua_tostring(coroutine, -1)
					);

					ScriptEngine::L::DumpStacktrace(coroutine);
				}
			}

			lua_unref(lua_mainthread(coroutine), corRef);

			ScriptEngine::s_YieldedCoroutines.erase(ScriptEngine::s_YieldedCoroutines.begin() + i);
		}

		// https://stackoverflow.com/a/17956637
		// asan was not happy about the iterator from
		// `::erase` for some reason?? TODO
		// 10/06/2025
		if (size != ScriptEngine::s_YieldedCoroutines.size())
		{
			i--;
			size = ScriptEngine::s_YieldedCoroutines.size();
		}
	}
}

void EcScript::Update(double dt)
{
	TIME_SCOPE_AS("Scripts");

	uint32_t ecId = this->EcId;

	// The first Script to be updated in the current frame will
	// need to handle resuming ALL the coroutines that were yielded,
	// the poor bastard
	// 23/09/2024
	resumeYieldedCoroutines();

	if (EcScript* after = static_cast<EcScript*>(ManagerInstance.GetComponent(ecId)); after && after != this)
	{
		// we got re-alloc'd, defer to clone to avoid use-after-free's
		after->Update(dt);
		return;
	}

	// we got destroy'd by the resumed coroutine
	if (!this->Object.Referred())
		return;
	if (this->Object->IsDestructionPending)
		return;

	ZoneScopedC(tracy::Color::LightSkyBlue);

	std::string fullname = this->Object->GetFullName();
	ZoneTextF("Script: %s\nFile: %s", fullname.c_str(), this->SourceFile.c_str());

	if (m_StaleSource)
		this->Reload();
}

bool EcScript::LoadScript(const std::string_view& scriptFile)
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	if (SourceFile == scriptFile)
		return true;

	this->SourceFile = scriptFile;
	m_StaleSource = true; // will be loaded on the next `::Update`

	return false;
}

bool EcScript::Reload()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	if (!LVM)
		LVM = ScriptEngine::L::Create("RootLVM");

	std::string fullName = this->Object->GetFullName();

	ZoneTextF("Script: %s\nFile: %s", fullName.c_str(), this->SourceFile.c_str());

	bool fileExists = true;
	m_Source = FileRW::ReadFile(SourceFile, &fileExists);

	m_StaleSource = false;

	if (m_L)
		lua_resetthread(m_L);
	else
		m_L = lua_newthread(LVM);

	luaL_sandboxthread(m_L);

	if (!fileExists)
	{
		Log::Error(
			std::format(
				"Script '{}' references invalid Source File '{}'!",
				fullName, this->SourceFile
			)
		);

		return false;
	}

	ScriptEngine::L::PushGameObject(m_L, this->Object);
	lua_setglobal(m_L, "script");

	int result = ScriptEngine::CompileAndLoad(m_L, m_Source, "@" + FileRW::MakePathCwdRelative(SourceFile));
	
	if (result == 0)
	{
		// prevent ourselves from being deleted by the code we run.
		// if that code errors, it'll be a use-after-free as we try
		// to access `m_L` to get the error message
		// 24/12/2024
		ObjectHandle dontKillMePlease = this->Object;
		// to prevent use-after-free on script error if we've gotten re-alloc'd
		lua_State* thread = m_L;

		int resumeResult = lua_resume(m_L, m_L, 0);

		if (resumeResult != LUA_OK && resumeResult != LUA_YIELD && resumeResult != LUA_BREAK)
		{
			Log::ErrorF(
				"Script init: {}",
				lua_tostring(thread, -1)
			);
			ScriptEngine::L::DumpStacktrace(thread);

			return false;
		}

		return true; /* return chunk main function */
	}
	else
	{
		Log::ErrorF(
			"Script compilation: {}",
			lua_tostring(m_L, -1)
		);

		return false;
	}

	return false;
}

static std::stack<lua_State*> s_LvmStack;

void EcScript::PushLVM(const std::string& VmName)
{
	if (s_LvmStack.size() == 0)
		s_LvmStack.push(LVM);
	
	s_LvmStack.push(ScriptEngine::L::Create(VmName));
	LVM = s_LvmStack.top();
}

void EcScript::PopLVM()
{
	if (s_LvmStack.size() == 1)
		RAISE_RT("Tried to pop Luau VMs with only 1 remaining");

	ScriptEngine::L::Close(LVM);
	
	s_LvmStack.pop();
	LVM = s_LvmStack.top();
}
