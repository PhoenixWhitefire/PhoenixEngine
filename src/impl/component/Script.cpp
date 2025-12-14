#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <luau/VM/include/lualib.h>
#include <tracy/Tracy.hpp>
#include <Luau/Compiler.h>
#include <format>
#include <stack>

#include "component/Script.hpp"
#include "script/ScriptEngine.hpp"
#include "script/luhx.hpp"
#include "datatype/Color.hpp"
#include "UserInput.hpp"
#include "Timing.hpp"
#include "FileRW.hpp"
#include "Memory.hpp"
#include "Log.hpp"

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
        static const Reflection::StaticPropertyMap props = {
            { "SourceFile", {
				Reflection::ValueType::String,
				REFLECTION_PROPERTY_GET_SIMPLE(EcScript, SourceFile),

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
        static const Reflection::StaticMethodMap funcs = {
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
};

static inline ScriptManager ManagerInstance{};

void EcScript::Update(double dt)
{
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

std::string EcScript::GetSource(bool* IsInline, bool* Success)
{
	if (SourceFile.size() > 0 && SourceFile[0] == '!' && strcmp("!File:", SourceFile.c_str()) != 0)
	{
		if (SourceFile.find("!InlineSource:") != std::string::npos)
		{
			if (Success)
				*Success = true;

			if (IsInline)
				*IsInline = true;

			size_t colonLoc = SourceFile.find_first_of(':');

			if (colonLoc + 1 < SourceFile.size())
				return SourceFile.substr(colonLoc + 1, SourceFile.size() - colonLoc - 1);
			else
				return "";
		}
		else
		{
			std::string error = std::format("Invalid SourceFile property '{}'", SourceFile);
			Log::Error(error);

			if (Success)
				*Success = false;

			return error;
		}
	}
	else
	{
		bool success = true;
		std::string contents = FileRW::ReadFile(SourceFile, &success);

		if (Success)
			*Success = success;

		if (IsInline)
			*IsInline = true;

		return contents;
	}
}

bool EcScript::Reload()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	std::string fullName = this->Object->GetFullName();
	ZoneTextF("Script: %s\nFile: %s", fullName.c_str(), SourceFile.c_str());

	ZoneTextF("Script: %s\nFile: %s", fullName.c_str(), SourceFile.c_str());

	bool readSuccess = true;
	std::string source = GetSource(nullptr, &readSuccess);

	m_StaleSource = false;

	if (m_L)
		lua_resetthread(m_L);
	else
		m_L = lua_newthread(ScriptEngine::GetCurrentVM().MainThread);

	luaL_sandboxthread(m_L);

	if (!readSuccess)
	{
		Log::ErrorF(
			"Failed to load '{}' for Script {}: {}",
			SourceFile, fullName, source // `source` will be the error message
		);

		return false;
	}

	luhx_pushgameobject(m_L, this->Object);
	lua_setglobal(m_L, "script");

	int result = ScriptEngine::CompileAndLoad(m_L, source, "@" + FileRW::MakePathCwdRelative(SourceFile));
	
	if (result == 0)
	{
		// prevent ourselves from being deleted by the code we run.
		// if that code errors, it'll be a use-after-free as we try
		// to access `m_L` to get the error message
		// 24/12/2024
		ObjectHandle dontKillMePlease = this->Object;
		// to prevent use-after-free on script error if we've gotten re-alloc'd
		lua_State* thread = m_L;

		int resumeResult = ScriptEngine::L::Resume(m_L, m_L, 0);

		if (resumeResult != LUA_OK && resumeResult != LUA_YIELD)
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
