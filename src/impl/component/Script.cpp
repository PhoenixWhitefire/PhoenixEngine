#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <luau/VM/include/lualib.h>
#include <tracy/Tracy.hpp>
#include <Luau/Compiler.h>
#include <luau/Require/Runtime/include/Luau/Require.h>
#include <format>

#include "component/Script.hpp"
#include "component/ScriptEngine.hpp"
#include "datatype/Color.hpp"
#include "UserInput.hpp"
#include "Timing.hpp"
#include "FileRW.hpp"
#include "Memory.hpp"
#include "Log.hpp"

static lua_State* LVM = nullptr;

class ScriptManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
		m_Components.back().Object = Object;
		m_Components.back().EcId = static_cast<uint32_t>(m_Components.size() - 1);

		return m_Components.back().EcId;
    }

	virtual void UpdateComponent(uint32_t Id, double DeltaTime) override
	{
		m_Components[Id].Update(DeltaTime);
	}

    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcScript& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

	virtual void* GetComponent(uint32_t Id) override
	{
		return &m_Components[Id];
	}

    virtual void DeleteComponent(uint32_t Id) override
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth
		//if (lua_State* L = m_Components[Id].m_L)
		//	lua_resetthread(L);
		
		m_Components[Id].Object.Invalidate();
    }

    virtual const Reflection::StaticPropertyMap& GetProperties() override
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

    virtual const Reflection::StaticMethodMap& GetMethods() override
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

    ScriptManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::Script] = this;
    }

	virtual void Shutdown() override
    {
        for (uint32_t i = 0; i < m_Components.size(); i++)
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

struct EventSignalData
{
	ReflectorHandle Reflector;
	const Reflection::Event* Event = nullptr;
};

struct EventConnectionData
{
	ReflectorHandle Reflector;
	const Reflection::Event* Event = nullptr;
	uint32_t ConnectionId = UINT32_MAX;
	int ThreadRef = LUA_NOREF;
	lua_State* L = nullptr;
	bool CallbackYields = false;
};

// modified version of `db_traceback` from `VM/src/ldblib.cpp`
static void dumpStacktrace(lua_State* L, std::string* Into = nullptr)
{
	lua_Debug ar;

	for (int i = (Into ? 1 : 0); lua_getinfo(L, i, "sln", &ar); i++)
	{
		std::string line = "[STCK]: from: ";

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

		if (!Into)
			Log::Append(line);
		else
		{
			Into->append(line);
			Into->append("\n");
		}
	}
}

static void pushSignal(lua_State* L, const Reflection::Event* Event, const ReflectorHandle& Reflector)
{
	EventSignalData* ev = (EventSignalData*)lua_newuserdata(L, sizeof(EventSignalData));
	ev->Reflector = Reflector;
	ev->Event = Event;

	luaL_getmetatable(L, "EventSignal");
	lua_setmetatable(L, -2);
}

static int api_newobject(lua_State* L)
{
	GameObject* newObject = GameObject::Create(luaL_checkstring(L, 1));
	ScriptEngine::L::PushGameObject(L, newObject);

	return 1;
};

static int api_gameobjindex(lua_State* L)
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

	std::pair<EntityComponent, uint32_t> reflectorHandle;

	if (const Reflection::Property* prop = obj->FindProperty(key, &reflectorHandle))
	{
		Reflection::GenericValue v = prop->Get(GameObject::ReflectorHandleToPointer(reflectorHandle));

		assert(
			(v.Type == prop->Type)
			|| (v.Type == Reflection::ValueType::Null && prop->Type == Reflection::ValueType::GameObject)
		);
		ScriptEngine::L::PushGenericValue(L, v);
	}

	else if (const Reflection::Event* event = obj->FindEvent(key, &reflectorHandle))
		pushSignal(L, event, reflectorHandle);

	// Methods are lower because we prefer namecalls
	else if (const Reflection::Method* func = obj->FindMethod(key, &reflectorHandle))
		ScriptEngine::L::PushMethod(L, func, reflectorHandle);

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

static int api_gameobjnewindex(lua_State* L)
{
	ZoneScopedNC("GameObject.__newindex", tracy::Color::LightSkyBlue);

	GameObject* obj = GameObject::FromGenericValue(ScriptEngine::L::LuaValueToGeneric(L, 1));
	const char* key = luaL_checkstring(L, 2);

	ZoneText(key, strlen(key));

	if (strcmp(key, "Exists") == 0)
		luaL_errorL(L, "%s", "'Exists' is read-only! - 21/12/2024");

	LUA_ASSERT(obj, "Tried to assign to the '%s' of a deleted Game Object", key);

	if (const Reflection::Property* prop = obj->FindProperty(key))
	{
		const char* argAsString = luaL_tolstring(L, 3, NULL);
		const char* argTypeName = luaL_typename(L, 3);
		
		if (!prop->Set)
			luaL_errorL(L, 
				"Cannot set Property '%s' to %s '%s' because it is read-only",
				key, argTypeName, argAsString
			);

		ScriptEngine::L::CheckType(L, prop->Type, 3);

		Reflection::GenericValue newValue = ScriptEngine::L::LuaValueToGeneric(L, 3);

		try
		{
			obj->SetPropertyValue(key, newValue);
		}
		catch (const std::runtime_error& err)
		{
			luaL_errorL(L, "Error while setting property %s '%s': %s", key, obj->Name.c_str(), err.what());
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

static int api_gameobjectnamecall(lua_State* L)
{
	ZoneScopedNC("GameObject.__namecall", tracy::Color::LightSkyBlue);

	GameObject* g = GameObject::FromGenericValue(ScriptEngine::L::LuaValueToGeneric(L, 1));
	const char* k = L->namecall->data; // this is weird 10/01/2025

	if (!g)
		luaL_errorL(L, "Tried to call '%s' of a de-allocated GameObject with ID %u", k, *(uint32_t*)lua_touserdata(L, 1));

	ZoneText(k, strlen(k));

	std::pair<EntityComponent, uint32_t> reflectorHandle;
	const Reflection::Method* func = g->FindMethod(k, &reflectorHandle);

	if (!func)
		luaL_errorL(L, "'%s' is not a valid method of %s", k, g->GetFullName().c_str());

	int numresults = 0;

	try
	{
		numresults = ScriptEngine::L::HandleMethodCall(
			L,
			func,
			reflectorHandle
		);
	}
	catch (const std::runtime_error& err)
	{
		luaL_errorL(L, "Error while invoking method '%s' of %s: %s", k, g->GetFullName().c_str(), err.what());
	}

	return numresults;
}

static int api_gameobjecttostring(lua_State* L)
{
	GameObject* object = GameObject::FromGenericValue(ScriptEngine::L::LuaValueToGeneric(L, 1));
	
	if (object)
		lua_pushstring(L, object->GetFullName().c_str());
	else
		lua_pushstring(L, "<!Deleted GameObject!>");

	return 1;
};

static int api_newcol(lua_State* L)
{
	float x = static_cast<float>(luaL_checknumber(L, 1));
	float y = static_cast<float>(luaL_checknumber(L, 2));
	float z = static_cast<float>(luaL_checknumber(L, 3));

	ScriptEngine::L::PushGenericValue(L, Color(x, y, z).ToGenericValue());

	return 1;
};

static int api_colindex(lua_State* L)
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

static int api_coltostring(lua_State* L)
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

static int api_eventnamecall(lua_State* L)
{
	if (strcmp(L->namecall->data, "Connect") == 0)
	{
		luaL_checktype(L, 2, LUA_TFUNCTION);

		EventSignalData* ev = (EventSignalData*)luaL_checkudata(L, 1, "EventSignal");
		const Reflection::Event* rev = ev->Event;

		// TRUST ME, ALL THESE L's MAKE SENSE OK
		// `eL` IS. UH. UHHH. *EVENT* L! YES, *E*VENT L
		// THEN, *THEN*, `cL` IS... *C*ONNECTION L!! YES, YES, YES!!!
		// "And then, `nL`?", YOU MAY ASK, (smart and kinda cute as always)
		// ...
		// ...
		// ...
		// ...
		// _*NNNNNNNNNNNNNNN*EW_ L! *NNNNNNN*EW!! *N*EW *N*EW *N*EW *N*EW!!!!!!!
		// MY GENIUS
		// UNPARAARALELLED
		//    ARAARA
		//    ara ara
		// (tee hee)
		lua_State* eL = lua_newthread(L);
		lua_State* cL = lua_newthread(eL);
		int threadRef = lua_ref(L, -1);
		lua_xpush(L, eL, 2);

		EventConnectionData* ec = (EventConnectionData*)lua_newuserdata(eL, sizeof(EventConnectionData));
		luaL_getmetatable(eL, "EventConnection"); // stack: ec, mt
		lua_setmetatable(eL, -2); // stack: ec

		lua_pushlightuserdata(eL, eL); // stack: ec, lud
		lua_pushvalue(eL, -2); // stack: ec, lud, ec
		lua_settable(eL, LUA_ENVIRONINDEX); // stack: ec
		lua_pop(eL, 1);
	
		uint32_t cnId = rev->Connect(
			GameObject::ReflectorHandleToPointer(ev->Reflector),
		
			[eL, cL, rev](const std::vector<Reflection::GenericValue>& Inputs)
			-> void
			{
				assert(Inputs.size() == rev->CallbackInputs.size());
				assert(lua_isfunction(eL, 2));

				lua_pushlightuserdata(eL, eL);
				lua_gettable(eL, LUA_ENVIRONINDEX);
				EventConnectionData* cn = (EventConnectionData*)luaL_checkudata(eL, -1, "EventConnection");
				lua_pop(eL, 1);

				lua_State* co = cL;

				// performance optimization:
				// if the callback never yields, then we know that
				// there cannot be more than one instance of it
				// running concurrently. thus, we can re-use a single thread
				// instead of creating a new one for each invocation
				if (cn->CallbackYields)
				{
					lua_State* nL = lua_newthread(eL);
					luaL_checkstack(nL, (int32_t)Inputs.size() + 1, "event connection callback args");
					lua_xpush(eL, nL, 2);
					lua_pop(eL, 1); // pop nL off eL

					co = nL;
				}
				else
				{
					lua_xpush(eL, cL, 2);
				}

				for (size_t i = 0; i < Inputs.size(); i++)
				{
					assert(Inputs[i].Type == rev->CallbackInputs[i]);
					ScriptEngine::L::PushGenericValue(co, Inputs[i]);
				}

				// TODO 11/08/2025
				// they added a "correct" way to do this, with continuations n stuff
				// but i genuinely just could not be bothered
				co->baseCcalls++;
				int status = lua_pcall(co, static_cast<int32_t>(Inputs.size()), 0, 0);
				co->baseCcalls++;

				if (status != LUA_OK && status != LUA_YIELD)
				{
					Log::ErrorF("Script event: {}", lua_tostring(co, -1));
					dumpStacktrace(co);
					lua_pop(co, 1);
				}

				// status returned from _pcall is 0 when it yields for some reason?? TODO
				if (status == LUA_YIELD || co->status == LUA_YIELD)
				{
					cn->CallbackYields = true;
				}
			}
		);

		ec->Reflector = ev->Reflector;
		ec->ThreadRef = threadRef;
		ec->ConnectionId = cnId;
		ec->Event = ev->Event;
		ec->L = L;
	
		return 1;
	}
	else
		luaL_error(L, "No such method of Event Signal known as '%s'", L->namecall->data);
}

static int api_evconnectionindex(lua_State* L)
{
	const char* k = luaL_checkstring(L, 2);
	EventConnectionData* ec = (EventConnectionData*)luaL_checkudata(L, 1, "EventConnection");

	if (strcmp(k, "Connected") == 0)
		lua_pushboolean(L, ec->ConnectionId != UINT32_MAX);

	else if (strcmp(k, "Signal") == 0)
		pushSignal(L, ec->Event, ec->Reflector);

	else
		luaL_error(L, "Invalid member '%s' of Event Connection", k);
				
	return 1;
}

static int api_evconnectionnamecall(lua_State* L)
{
	if (strcmp(L->namecall->data, "Disconnect") == 0)
	{
		EventConnectionData* ec = (EventConnectionData*)luaL_checkudata(L, 1, "EventConnection");

		if (ec->ConnectionId == UINT32_MAX)
			luaL_error(L, "Event Connection was already disconnected!");

		ec->Event->Disconnect(GameObject::ReflectorHandleToPointer(ec->Reflector), ec->ConnectionId);
		ec->ConnectionId = UINT32_MAX;

		lua_pushlightuserdata(L, L);
		lua_pushnil(L);
		lua_settable(L, LUA_ENVIRONINDEX); // remove stacktrace string

		lua_unref(ec->L, ec->ThreadRef);
	}
	else
		luaL_error(L, "No such method of Event Connection known as '%s'", L->namecall->data);
	
	return 0;
}

static void* l_alloc(void*, void* ptr, size_t, size_t nsize)
{
	if (nsize == 0) {
		Memory::Free(ptr);
		return NULL;
	}
	else
		return Memory::ReAlloc(ptr, nsize, Memory::Category::Luau);
}

static int lprint(lua_State* L)
{
	// FROM:
	// `luaB_print`
	// `Luau/VM/src/lbaselib.cpp`
	// 11/11/2024

	if (lua_Debug ar; lua_getinfo(L, 1, "sl", &ar))
		Log::Info("&&", std::format("[S{}:{}]", ar.short_src, ar.currentline));

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
}

static void requireConfigInit(luarequire_Configuration* config)
{
	config->is_require_allowed = [](lua_State*, void*, const char*)
		{
			return true;
		};
	config->reset = [](lua_State*, void* ctx, const char* chname)
		{
			// chunkname is prefixed with @
			assert(chname[0] == '@');
			((std::filesystem::path*)ctx)->assign(chname + 1);
			return NAVIGATE_SUCCESS;
		};
	config->jump_to_alias = [](lua_State*, void*, const char*)
		{
			return NAVIGATE_NOT_FOUND;
		};
	config->to_parent = [](lua_State*, void* ctx)
		{
			std::filesystem::path* curpath = (std::filesystem::path*)ctx;

			if (curpath->has_parent_path())
			{
				*curpath = curpath->parent_path();
				return NAVIGATE_SUCCESS;
			}
			else
				return NAVIGATE_NOT_FOUND;
		};
	config->to_child = [](lua_State*, void* ctx, const char* name)
		{
			std::filesystem::path* curpath = (std::filesystem::path*)ctx;
			if (!std::filesystem::exists(*curpath / name))
				return NAVIGATE_NOT_FOUND;

			*curpath /= name;
			return NAVIGATE_SUCCESS;
		};
	config->is_module_present = [](lua_State*, void* ctx)
		{
			std::filesystem::path* curpath = (std::filesystem::path*)ctx;

			if (std::filesystem::is_regular_file(*curpath))
				return true;
			else
				return false;
		};
	config->get_chunkname = [](lua_State*, void* ctx, char* buffer, size_t bufferSize, size_t* outSize)
		{
			std::filesystem::path* curpath = (std::filesystem::path*)ctx;
			std::string strpath = curpath->string();
			*outSize = strpath.size();

			if (bufferSize < strpath.size())
				return WRITE_BUFFER_TOO_SMALL;
			else
			{
				memcpy(buffer, strpath.data(), strpath.size());
				return WRITE_SUCCESS;
			}
		};
	config->get_loadname = config->get_chunkname; // TODO what's a loadname
	config->get_cache_key = config->get_chunkname;
	config->is_config_present = [](lua_State*, void* ctx)
		{
			return std::filesystem::is_regular_file(*(std::filesystem::path*)ctx / ".luaurc");
		};
	config->get_config = [](lua_State*, void* ctx, char* buffer, size_t bufferSize, size_t* outSize)
		{
			std::filesystem::path* curpath = (std::filesystem::path*)ctx;
			std::string contents = FileRW::ReadFile(curpath->string());
			*outSize = contents.size();

			if (bufferSize < contents.size())
				return WRITE_BUFFER_TOO_SMALL;
			
			memcpy(buffer, contents.data(), contents.size());

			return WRITE_SUCCESS;
		};
	config->load = [](lua_State* L, void* ctx, const char* path, const char* chname, const char* ldname)
		{
			std::filesystem::path* curpath = (std::filesystem::path*)ctx;

			// from `Luau/CLI/src/ReplRequirer.cpp` 13/08/2025

			// module needs to run in a new thread, isolated from the rest
			// note: we create ML on main thread so that it doesn't inherit environment of L
			lua_State* GL = lua_mainthread(L);
			lua_State* ML = lua_newthread(GL);
			lua_xmove(GL, L, 1);

			// new thread needs to have the globals sandboxed
			luaL_sandboxthread(ML);
			
			if (ScriptEngine::CompileAndLoad(ML, FileRW::ReadFile(curpath->string()), chname) == 0)
			{
				int status = lua_resume(ML, L, 0);

				if (status == 0)
				{
					if (lua_gettop(ML) == 0)
						lua_pushstring(ML, "module must return a value");

					else if (!lua_istable(ML, -1) && !lua_isfunction(ML, -1))
						lua_pushstring(ML, "module must return a table or function");
				}
				else if (status == LUA_YIELD)
					lua_pushstring(ML, "module can not yield");

				else if (!lua_isstring(ML, -1))
					lua_pushstring(ML, "unknown error while running module");
			}
			
			// add ML result to L stack
    		lua_xmove(ML, L, 1);
    		if (lua_isstring(L, -1))
    		    lua_error(L);

    		// remove ML thread from L stack
    		lua_remove(L, -2);

    		// added one value to L stack: module result
			return 1;
		};
}

static lua_State* createVM()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	lua_State* state = lua_newstate(l_alloc, nullptr);
	// Load Standard Library ('print' etc
	luaL_openlibs(state);

	luaopen_require(
		state,
		requireConfigInit,
		new std::filesystem::path()
	);

	// override std `print` to use logging system
	lua_pushcfunction(
		state,
		lprint,
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
			api_gameobjectnamecall,
			"__namecall" // leaving as "__namecall" SPECIFICALLY adds the method name to errors (check `currfuncname` in laux.cpp)
		);
		lua_setfield(state, -2, "__namecall");

		lua_pushcfunction(state, api_gameobjecttostring, "GameObject.__tostring");
		lua_setfield(state, -2, "__tostring");

		lua_pushstring(state, "GameObject");
		lua_setfield(state, -2, "__type");
	}

	// Event Signal
	{
		luaL_newmetatable(state, "EventSignal");

		lua_pushcfunction(
			state,
			api_eventnamecall,
			"__namecall"
		);
		lua_setfield(state, -2, "__namecall");

		lua_pushstring(state, "EventSignal");
		lua_setfield(state, -2, "__type");
	}

	// Event Connection
	{
		luaL_newmetatable(state, "EventConnection");

		lua_pushcfunction(
			state,
			api_evconnectionnamecall,
			"__namecall"
		);
		lua_setfield(state, -2, "__namecall");

		lua_pushcfunction(
			state,
			api_evconnectionindex,
			"EventConnection.__index"
		);
		lua_setfield(state, -2, "__index");

		lua_pushstring(state, "EventConnection");
		lua_setfield(state, -2, "__type");
	}

	ScriptEngine::L::PushGameObject(state, GameObject::GetObjectById(GameObject::s_DataModel));
	lua_setglobal(state, "game");

	ScriptEngine::L::PushGameObject(state, GameObject::GetObjectById(GameObject::s_DataModel)->FindChild("Workspace"));
	lua_setglobal(state, "workspace");

	for (size_t i = 0; ScriptEngine::L::GlobalFunctions[i].second.Function != nullptr; i++)
	{
		std::pair<std::string_view, ScriptEngine::L::GlobalFn>& pair = ScriptEngine::L::GlobalFunctions[i];
		ScriptEngine::L::GlobalFn& func = pair.second;

		lua_pushlightuserdata(state, (void*)&func);
		lua_pushstring(state, pair.first.data());
		
		lua_pushcclosure(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("PhoenixGlobals", tracy::Color::LightSkyBlue);

				const char* fnName = lua_tostring(L, lua_upvalueindex(2));

				ZoneText(fnName, strlen(fnName));

				ScriptEngine::L::GlobalFn* func = (ScriptEngine::L::GlobalFn*)lua_touserdata(L, lua_upvalueindex(1));

				try
				{
					if (int nargs = lua_gettop(L); nargs >= func->NumMinArgs)
						return func->Function(L);
					else
						luaL_error(
							L,
							"Function `%s` expects at least %i arguments, but got only %i",
							fnName,
							func->NumMinArgs,
							nargs
						);
				}
				catch (const std::runtime_error& e)
				{
					luaL_errorL(L, "Error while invoking global function '%s': %s", fnName, e.what());
				}

			},
			pair.first.data(),
			2
		);
		lua_setglobal(state, pair.first.data());
	}

	return state;
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

				int resumeStatus = lua_resume(coroutine, nullptr, (int32_t)returnVals.size());

				if (resumeStatus != LUA_OK && resumeStatus != LUA_YIELD)
				{
					Log::ErrorF(
						"Script resumption: {}",
						lua_tostring(coroutine, -1)
					);

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
	if (!GameObject::GetObjectById(this->Object.m_TargetId))
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

	int result = ScriptEngine::CompileAndLoad(m_L, m_Source, "@" + FileRW::TryMakePathCwdRelative(SourceFile));
	
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
			Log::ErrorF(
				"Script init: {}",
				lua_tostring(thread, -1)
			);
			dumpStacktrace(thread);

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
