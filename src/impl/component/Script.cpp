﻿#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <luau/VM/include/lualib.h>
#include <tracy/Tracy.hpp>
#include <Luau/Compiler.h>

#include "component/Script.hpp"
#include "component/ScriptEngine.hpp"
#include "datatype/Color.hpp"
#include "UserInput.hpp"
#include "Timing.hpp"
#include "FileRW.hpp"
#include "Memory.hpp"
#include "Log.hpp"

static lua_State* LVM = nullptr;

class ScriptManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) final
    {
        m_Components.emplace_back();
		m_Components.back().Object = Object;
		m_Components.back().EcId = static_cast<uint32_t>(m_Components.size() - 1);

		return m_Components.back().EcId;
    }

	virtual void UpdateComponent(uint32_t Id, double DeltaTime) final
	{
		m_Components[Id].Update(DeltaTime);
	}

    virtual std::vector<void*> GetComponents() final
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcScript& t : m_Components)
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
		if (lua_State* L = m_Components[Id].m_L)
			lua_resetthread(L);
		
		m_Components[Id].Object.Invalidate();
    }

    virtual const Reflection::PropertyMap& GetProperties() final
    {
        static const Reflection::PropertyMap props = 
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

    virtual const Reflection::FunctionMap& GetFunctions() final
    {
        static const Reflection::FunctionMap funcs =
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

    ScriptManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::Script] = this;
    }

	virtual void Shutdown() final
    {
        for (size_t i = 0; i < m_Components.size(); i++)
            DeleteComponent(i);
		
		if (LVM)
			lua_close(LVM);
		LVM = nullptr;
    }

private:
    std::vector<EcScript> m_Components;
};

static inline ScriptManager ManagerInstance{};

#define LUA_ASSERT(res, err, ...) { if (!(res)) { luaL_errorL(L, err, __VA_ARGS__); } }

static auto api_newobject = [](lua_State* L)
	{
		GameObject* newObject = GameObject::Create(luaL_checkstring(L, 1));
		ScriptEngine::L::PushGameObject(L, newObject);

		return 1;
	};

static auto api_gameobjindex = [](lua_State* L)
	{
		ZoneScopedNC("GameObject.__index", tracy::Color::LightSkyBlue);

		GameObject* obj = GameObject::FromGenericValue(ScriptEngine::L::LuaValueToGeneric(L, 1));
		const char* key = luaL_checkstring(L, 2);

		ZoneText(key, strlen(key));

		if (strcmp(key, "Exists") == 0)
		{
			// whether or not it exists
			lua_pushboolean(L, obj ? 1 : 0);
			return 1;
		}

		LUA_ASSERT(obj, "Tried to index '%s' of a deleted Game Object (use '.Exists' to check before accessing a member)", key);

		std::pair<EntityComponent, uint32_t> componentHandle;

		if (const Reflection::Property* prop = obj->FindProperty(key, &componentHandle))
		{
			void* p = componentHandle.first != EntityComponent::None
				? GameObject::ComponentHandleToPointer(componentHandle)
				: obj;
			
			ScriptEngine::L::PushGenericValue(L, prop->Get(p));
		}

		else if (Reflection::Function* func = obj->FindFunction(key))
			ScriptEngine::L::PushFunction(L, func);

		else
		{
			GameObject* child = obj->FindChild(key);

			if (child)
				ScriptEngine::L::PushGameObject(L, child);
			else
				// 18/05/2025
				// this is going to be an error because i spent an entire 26 seconds
				// trying to figure out why something wasnt working
				luaL_errorL(L, "No child or member '%s' of %s", key, obj->GetFullName().c_str());
		}

		return 1;
	};

static auto api_gameobjnewindex = [](lua_State* L)
	{
		ZoneScopedNC("GameObject.__newindex", tracy::Color::LightSkyBlue);

		GameObject* obj = GameObject::FromGenericValue(ScriptEngine::L::LuaValueToGeneric(L, 1));
		const char* key = luaL_checkstring(L, 2);

		ZoneText(key, strlen(key));

		if (strcmp(key, "Exists") == 0)
			luaL_errorL(L, "%s", "'Exists' is read-only! - 21/12/2024");

		LUA_ASSERT((obj), "Tried to assign to the '%s' of a deleted Game Object", key);

		if (Reflection::Property* prop = obj->FindProperty(key))
		{
			const char* argAsString = luaL_tolstring(L, -1, NULL);
			const char* argTypeName = luaL_typename(L, -1);
			// 14/09/2024
			// Lua what the fuck
			// Please stop flip-flopping between negative and positive indexes for
			// stack offsets! What is the point! Why?!
			lua_Type argType = (lua_Type)lua_type(L, 3);
			
			if (!prop->Set)
				luaL_errorL(L, 
					"Cannot set Property '%s' to %s '%s' because it is read-only",
					key, argTypeName, argAsString
				);

			auto reflToLuaIt = ScriptEngine::ReflectedTypeLuaEquivalent.find(prop->Type);

			if (reflToLuaIt == ScriptEngine::ReflectedTypeLuaEquivalent.end())
			{
				const std::string_view& typeName = Reflection::TypeAsString(prop->Type);
				luaL_errorL(L,
					"No defined mapping between a '%s' and a Lua type",
					typeName.data()
				);
			}

			lua_Type desiredType = reflToLuaIt->second;

			if (desiredType != argType && (desiredType != LUA_TUSERDATA && argType != LUA_TNIL))
			{
				const char* desiredTypeName = lua_typename(L, (int)desiredType);
				luaL_errorL(L, 
					"Expected type %s for member '%s', got %s instead",
					desiredTypeName, key, argTypeName
				);
			}
			else
			{
				lua_pushvalue(L, 3);

				Reflection::GenericValue newValue = ScriptEngine::L::LuaValueToGeneric(L);

				try
				{
					obj->SetPropertyValue(key, newValue);
				}
				catch (std::string err)
				{
					luaL_errorL(L, "Error while setting property %s '%s': %s", key, obj->Name.c_str(), err.c_str());
				}
			}
		}
		else
		{
			std::string fullname = obj->GetFullName();

			if (obj->FindChild(key))
				luaL_errorL(L,
					"Attempt to set invalid Member '%s' of '%s', although it has a child object with that name",
					key, fullname.c_str()
				);
			else
				luaL_errorL(L,
					"Attempt to set invalid Member '%s' of '%s'",
					key, fullname.c_str()
				);
		}

		return 0;
	};

static auto api_gameobjecttostring = [](lua_State* L)
	{
		GameObject* object = GameObject::FromGenericValue(ScriptEngine::L::LuaValueToGeneric(L, 1));
		
		if (object)
			lua_pushstring(L, object->GetFullName().c_str());
		else
			lua_pushstring(L, "<!Deleted GameObject!>");

		return 1;
	};

static auto api_newcol = [](lua_State* L)
	{
		float x = static_cast<float>(luaL_checknumber(L, 1));
		float y = static_cast<float>(luaL_checknumber(L, 2));
		float z = static_cast<float>(luaL_checknumber(L, 3));

		ScriptEngine::L::PushGenericValue(L, Color(x, y, z).ToGenericValue());

		return 1;
	};

static auto api_colindex = [](lua_State* L)
	{
		Color* vec = (Color*)luaL_checkudata(L, 1, "Color");
		const char* key = luaL_checkstring(L, 2);

		lua_getglobal(L, "Color");
		lua_pushstring(L, key);
		lua_rawget(L, -2);

		// Pass-through to Vector3.new
		if (!lua_isnil(L, -1))
			return 1;

		if (strcmp(key, "R") == 0)
		{
			lua_pushnumber(L, vec->R);
			return 1;
		}
		else if (strcmp(key, "G") == 0)
		{
			lua_pushnumber(L, vec->G);
			return 1;
		}
		else if (strcmp(key, "B") == 0)
		{
			lua_pushnumber(L, vec->B);
			return 1;
		}
		else
			luaL_errorL(L, "Invalid key %s", key);
	};

static auto api_coltostring = [](lua_State* L)
	{
		Color* col = (Color*)luaL_checkudata(L, 1, "Color");

		lua_pushstring(
			L,
			std::format(
				"{}, {}, {}",
				col->R, col->G, col->B
			).c_str()
		);

		return 1;
	};

static void* l_alloc(void*, void* ptr, size_t, size_t nsize)
{
	if (nsize == 0) {
		Memory::Free(ptr);
		return NULL;
	}
	else
		return Memory::ReAlloc(ptr, nsize, Memory::Category::Luau);
}

static lua_State* createVM()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	lua_State* state = lua_newstate(l_alloc, nullptr);
	// Load Standard Library ('print' etc
	luaL_openlibs(state);
	luaopen_vector(state); // vec lib not opened by default when i checked? even though it's in the `lualibs` array??

	// override std `print` to use logging system
	lua_pushcfunction(
		state,
		[](lua_State* L)
		{
			// FROM:
			// `luaB_print`
			// `Luau/VM/src/lbaselib.cpp`
			// 11/11/2024

			if (lua_Debug ar; lua_getinfo(L, 1, "sl", &ar))
			{
				std::string extraTag = std::format("[S{}:{}]", ar.short_src, ar.currentline);
				Log::Info("&&", extraTag);
			}
			else
				Log::Warning("Could not `lua_getinfo` for the following Luau log:");

			int n = lua_gettop(L); // number of arguments
			for (int i = 1; i <= n; i++)
			{
				size_t l;
				const char* s = luaL_tolstring(L, i, &l); // convert to string using __tostring et al

				if (i > 1)
					Log::Append(" &&");
				else
					Log::Append("&&");

				Log::Append(std::string(s) + "&&");
				lua_pop(L, 1); // pop result
			}
			Log::Append("\n&&");

			return 0;
		},
		"PhxPrintOverride"
	);
	lua_setglobal(state, "print");

	// Color
	{
		lua_newtable(state);

		lua_pushcfunction(state, api_newcol, "Color.new");
		lua_setfield(state, -2, "new");

		lua_setglobal(state, "Color");

		luaL_newmetatable(state, "Color");

		lua_pushcfunction(state, api_colindex, "Color.__index");
		lua_setfield(state, -2, "__index");

		lua_pushcfunction(state, api_coltostring, "Color.__tostring");
		lua_setfield(state, -2, "__tostring");

		lua_pushstring(state, "Color");
		lua_setfield(state, -2, "__type");
	}

	// Matrix 
	{
		lua_newtable(state);

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				Reflection::GenericValue gv{ glm::mat4(1.f) };
				ScriptEngine::L::PushGenericValue(L, gv);

				return 1;
			},
			"Matrix.new"
		);
		lua_setfield(state, -2, "new");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("Matrix.fromTranslation", tracy::Color::LightSkyBlue);

				glm::mat4 m(1.f);

				int numArgs = lua_gettop(L);

				switch (numArgs)
				{
				case 1:
				{
					const float* vec = luaL_checkvector(L, -1);
					m[3] = glm::vec4(glm::make_vec3(vec), 1.f);

					break;
				}
				case 3:
				{
					float x = static_cast<float>(luaL_checknumber(L, 1));
					float y = static_cast<float>(luaL_checknumber(L, 2));
					float z = static_cast<float>(luaL_checknumber(L, 3));

					m[3] = glm::vec4(glm::vec3(x, y, z), 1.f);

					break;
				}

				default:
					luaL_errorL(
						L,
						"`Matrix.fromTranslation` expected 1 or 3 arguments, got %i",
						numArgs
					);
				}
				
				ScriptEngine::L::PushGenericValue(L, m);

				return 1;
			},
			"Matrix.fromTranslation"
		);
		lua_setfield(state, -2, "fromTranslation");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("Matrix.fromEulerAnglesXYZ", tracy::Color::LightSkyBlue);

				float x = static_cast<float>(luaL_checknumber(L, 1));
				float y = static_cast<float>(luaL_checknumber(L, 2));
				float z = static_cast<float>(luaL_checknumber(L, 3));

				glm::mat4 t(1.f);
				t = glm::rotate(t, x, glm::vec3(1.f, 0.f, 0.f));
				t = glm::rotate(t, y, glm::vec3(0.f, 1.f, 0.f));
				t = glm::rotate(t, z, glm::vec3(0.f, 0.f, 1.f));

				ScriptEngine::L::PushGenericValue(L, t);

				return 1;
			},
			"Matrix.fromEulerAnglesXYZ"
		);
		lua_setfield(state, -2, "fromEulerAnglesXYZ");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				const float* a = luaL_checkvector(L, 1);
				const float* b = luaL_checkvector(L, 2);

				ScriptEngine::L::PushGenericValue(
					L,
					glm::lookAt(glm::make_vec3(a), glm::make_vec3(b), glm::vec3(0.f, 1.f, 0.f))
				);

				return 1;
			},
			"Matrix.lookAt"
		);
		lua_setfield(state, -2, "lookAt");

		lua_setglobal(state, "Matrix");

		luaL_newmetatable(state, "Matrix");

		lua_pushstring(state, "Matrix");
		lua_setfield(state, -2, "__type");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("Matrix.__index", tracy::Color::LightSkyBlue);

				glm::mat4& m = *(glm::mat4*)luaL_checkudata(L, 1, "Matrix");
				const char* k = luaL_checkstring(L, 2);

				ZoneText(k, strlen(k));

				if (strcmp(k, "Position") == 0)
					ScriptEngine::L::PushGenericValue(
						L,
						glm::vec3(m[3])
					);
				else if (strcmp(k, "Forward") == 0)
					ScriptEngine::L::PushGenericValue(
						L,
						glm::normalize(glm::vec3(m[2]))
					);
				else if (strcmp(k, "Up") == 0)
					ScriptEngine::L::PushGenericValue(
						L,
						glm::normalize(glm::vec3(m[1]))
					);
				else if (strcmp(k, "Right") == 0)
					ScriptEngine::L::PushGenericValue(
						L,
						glm::normalize(glm::vec3(m[0]))
					);
				else
					luaL_errorL(L, "Invalid member %s", k);

				return 1;
			},
			"Matrix.__index"
		);
		lua_setfield(state, -2, "__index");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				glm::mat4& a = *(glm::mat4*)luaL_checkudata(L, 1, "Matrix");
				glm::mat4& b = *(glm::mat4*)luaL_checkudata(L, 2, "Matrix");

				ScriptEngine::L::PushGenericValue(L, a * b);

				return 1;
			},
			"Matrix.__mul"
		);
		lua_setfield(state, -2, "__mul");
	}

	// GameObject
	{
		lua_newtable(state);

		lua_pushcfunction(state, api_newobject, "GameObject.new");
		lua_setfield(state, -2, "new");

		lua_setglobal(state, "GameObject");

		luaL_newmetatable(state, "GameObject");

		lua_pushcfunction(state, api_gameobjindex, "GameObject.__index");
		lua_setfield(state, -2, "__index");

		lua_pushcfunction(state, api_gameobjnewindex, "GameObject.__newindex");
		lua_setfield(state, -2, "__newindex");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("GameObject.__namecall", tracy::Color::LightSkyBlue);

				GameObject* g = GameObject::FromGenericValue(ScriptEngine::L::LuaValueToGeneric(L, 1));
				const char* k = L->namecall->data; // this is weird 10/01/2025

				if (!g)
					luaL_errorL(L, "Tried to call '%s' of a de-allocated GameObject with ID %u", k, *(uint32_t*)lua_touserdata(L, 1));

				ZoneText(k, strlen(k));

				Reflection::Function* func = g->FindFunction(k);

				if (!func)
					luaL_errorL(L, "'%s' is not a valid method of %s", k, g->GetFullName().c_str());

				int numresults = 0;

				try
				{
					numresults = ScriptEngine::L::HandleMethodCall(
						L,
						func
					);
				}
				catch (std::string err)
				{
					luaL_errorL(L, "Error while invoking method '%s' of %s: %s", k, g->GetFullName().c_str(), err.c_str());
				}

				return numresults;
			},
			"GameObject.__namecall"
		);
		lua_setfield(state, -2, "__namecall");

		lua_pushcfunction(state, api_gameobjecttostring, "GameObject.__tostring");
		lua_setfield(state, -2, "__tostring");

		lua_pushstring(state, "GameObject");
		lua_setfield(state, -2, "__type");
	}

	ScriptEngine::L::PushGameObject(state, GameObject::GetObjectById(GameObject::s_DataModel));
	lua_setglobal(state, "game");

	ScriptEngine::L::PushGameObject(state, GameObject::GetObjectById(GameObject::s_DataModel)->FindChild("Workspace"));
	lua_setglobal(state, "workspace");

	for (size_t i = 0; ScriptEngine::L::GlobalFunctions[i].second != nullptr; i++)
	{
		const std::pair<std::string_view, lua_CFunction>& pair = ScriptEngine::L::GlobalFunctions[i];
		lua_CFunction func = pair.second;

		lua_pushlightuserdata(state, (void*)func);
		lua_pushstring(state, pair.first.data());
		
		lua_pushcclosure(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("PhoenixGlobals", tracy::Color::LightSkyBlue);

				const char* fnName = lua_tostring(L, lua_upvalueindex(2));

				ZoneText(fnName, strlen(fnName));

				lua_CFunction func = (lua_CFunction)lua_touserdata(L, lua_upvalueindex(1));

				try
				{
					return func(L);
				}
				catch (std::string e)
				{
					luaL_errorL(L, "Error while invoking global function '%s': %s", fnName, e.c_str());
				}

			},
			pair.first.data(),
			2
		);
		lua_setglobal(state, pair.first.data());
	}

	return state;
}

// modified version of `db_traceback` from `VM/src/ldblib.cpp`
static void dumpStacktrace(lua_State* L)
{
	lua_Debug ar;

	for (int i = 1; lua_getinfo(L, i, "sln", &ar); i++)
	{
		std::string line = "[STCK]: from: ";

		if (strcmp(ar.what, "C") == 0)
		{
			line.append("[C]");
			Log::Append(line);
			continue;
		}

		if (ar.source)
			line.append(ar.short_src);
		
		if (ar.currentline > 0)
		{
			line.append(":");
			line.append(std::to_string(ar.currentline));
		}

		if (ar.name)
		{
			line.append(" in ");
			line.append(ar.name);
		}

		Log::Append(line);
	}
}

static bool shouldResume_Scheduled(
	const ScriptEngine::YieldedCoroutine& CorInfo,
	std::vector<Reflection::GenericValue>* ReturnValues
)
{
	if (double curTime = GetRunningTime(); curTime >= CorInfo.RmSchedule.ResumeAt)
	{
		ReturnValues->emplace_back(curTime - CorInfo.RmSchedule.YieldedAt);

		return true;
	}
	else
		return false;
}

static bool shouldResume_Future(const ScriptEngine::YieldedCoroutine& CorInfo, std::vector<Reflection::GenericValue>* ReturnValues)
{
	const std::shared_future<std::vector<Reflection::GenericValue>>& future = CorInfo.RmFuture;

	if ( future.valid()
		 && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready
	)
	{
		*ReturnValues = future.get();

		return true;
	}
	else
		return false;
}

static bool shouldResume_Polled(const ScriptEngine::YieldedCoroutine& CorInfo, std::vector<Reflection::GenericValue>* ReturnValues)
{
	const std::function<bool(std::vector<Reflection::GenericValue>*)> checkStatus = CorInfo.RmPoll;

	if (checkStatus(ReturnValues))
		return true;
	else
		return false;
}

typedef bool(*ResumptionModeHandler)(const ScriptEngine::YieldedCoroutine&, std::vector<Reflection::GenericValue>*);

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

		std::vector<Reflection::GenericValue> returnVals;
		bool shouldResume = handler(*it, &returnVals);

		if (shouldResume)
		{
			// make sure the script still exists
			// modules don't have a `script` global, but they run on their own coroutine independent from
			// where they are `require`'d from anyway
			if (it->ScriptId == PHX_GAMEOBJECT_NULL_ID || GameObject::GetObjectById(it->ScriptId))
			{
				for (const Reflection::GenericValue& v : returnVals)
					ScriptEngine::L::PushGenericValue(coroutine, v);

				int resumeStatus = lua_resume(coroutine, nullptr, returnVals.size());

				if (resumeStatus != LUA_OK && resumeStatus != LUA_YIELD)
				{
					Log::Error(std::format(
						"Script resumption: {}",
						lua_tostring(coroutine, -1)
					));

					dumpStacktrace(coroutine);
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

	s_WindowGrabMouse = ScriptEngine::s_BackendScriptWantGrabMouse;

	uint32_t ecId = this->EcId;

	// The first Script to be updated in the current frame will
	// need to handle resuming ALL the coroutines that were yielded,
	// the poor bastard
	// 23/09/2024
	resumeYieldedCoroutines();

	if (EcScript* after = static_cast<EcScript*>(ManagerInstance.GetComponent(ecId)); after != this)
	{
		// we got re-alloc'd, defer to clone to avoid use-after-free's
		after->Update(dt);
		return;
	}

	// we got destroy'd by the resumed coroutine
	if (this->Object->IsDestructionPending)
		return;

	ZoneScopedC(tracy::Color::LightSkyBlue);

	std::string fullname = this->Object->GetFullName();
	ZoneTextF("Script: %s\nFile: %s", fullname.c_str(), this->SourceFile.c_str());

	if (m_StaleSource)
	{
		this->Reload();
		return;
	}

	// script has failed to load (compilation error) 07/11/2024
	if (!m_L)
		return;

	if (lua_status(m_L) != LUA_OK)
		return;
	
	luaL_checkstack(m_L, 4, NULL);

	lua_getglobal(m_L, "Update");

	if (lua_isfunction(m_L, -1))
	{
		lua_State* co = lua_newthread(m_L);

		lua_getglobal(co, "Update");
		lua_pushnumber(co, dt);

		// why do all of these functions say they return `int` and not
		// `lua_Status` like they actually do? 23/09/2024
		int updateStatus = lua_resume(co, m_L, 1);

		// 10/04/2025
		// maybe `this` will eventually be a (for example) Ref<EcScript> in the future
		// so i don't have to do this bs
		EcScript* after = static_cast<EcScript*>(ManagerInstance.GetComponent(ecId));

		if (updateStatus != LUA_OK && updateStatus != LUA_YIELD)
		{
			const char* errstr = lua_tostring(co, -1);

			Log::Error(std::format(
				"Script tick: {}",
				errstr
			));
			dumpStacktrace(co);

			lua_resetthread(co);
			after->m_L = nullptr;
		}
		else if (updateStatus == LUA_OK)
		{
			lua_resetthread(co);
			lua_pop(after->m_L, 2);
		}
	}
	else
		lua_pop(m_L, 1);
}

bool EcScript::LoadScript(const std::string_view& scriptFile)
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	if (SourceFile == scriptFile)
		return true;

	this->SourceFile = scriptFile;
	m_StaleSource = true;

	/*
		Don't want the script to run before SceneFormat has fully
		deserialized the scene and the `script` global to
		report it is parented to `nil`. Only want it to run when
		parented under the `DataModel`. An `::IsDescendantOf` would be
		ideal, but it's not needed right now as this value will always
		be set before Script get's its GameObject properties assigned
		(i.e. it's Parent).

		Manually call `::Reload` after `::LoadScript` to bypass this
		restriction.
	*/
	if (!this->Object->GetParent())
		return false;
	else
		return this->Reload();
}

bool EcScript::Reload()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	if (!LVM)
		LVM = createVM();

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

	int result = ScriptEngine::CompileAndLoad(m_L, m_Source, "@" + fullName);
	
	if (result == 0)
	{
		// prevent ourselves from being deleted by the code we run.
		// if that code errors, it'll be a use-after-free as we try
		// to access `m_L` to get the error message
		// 24/12/2024
		GameObjectRef dontKillMePlease = this->Object;
		// to prevent use-after-free on script error if we've gotten re-alloc'd
		lua_State* thread = m_L;

		int resumeResult = lua_resume(m_L, m_L, 0);

		if (resumeResult != LUA_OK && resumeResult != LUA_YIELD)
		{
			Log::Error(std::format(
				"Script init: {}",
				lua_tostring(thread, -1)
			));
			dumpStacktrace(thread);

			return false;
		}

		return true; /* return chunk main function */
	}
	else
	{
		Log::Error(std::format(
			"Script compilation: {}",
			lua_tostring(m_L, -1)
		));

		return false;
	}

	return false;
}
