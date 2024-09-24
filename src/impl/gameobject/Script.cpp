/*
	
	11/09/2024:
	This is a mess. If you see any references to `Reflection::Reflectable`,
	`Reflection::IProperty` or `Reflection::IFunction`, just translate them
	in your head to `GameObject`, `IProperty` and `IFunction`. They are different,
	and I have taken the approach of trying to reduce any pre-emptive abstractions

*/

#include<glm/gtc/matrix_transform.hpp>
#include<luau/VM/include/lualib.h>

#include"gameobject/Script.hpp"
#include"datatype/GameObject.hpp"
#include"datatype/Vector3.hpp"
#include"UserInput.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"
#include"gameobject/ScriptEngine.hpp"

#define LUA_ASSERT(res, err) if (!res) { luaL_error(L, err); }

static RegisterDerivedObject<Object_Script> RegisterClassAs("Script");

static bool s_DidInitReflection = false;
static lua_State* DefaultState = nullptr;

static auto api_newobject = [](lua_State* L)
	{
		GameObject* newObject = GameObject::Create(luaL_checkstring(L, 1));
		ScriptEngine::L::PushGameObject(L, newObject);

		return 1;
	};

static auto api_gameobjindex = [](lua_State* L)
	{
		uint32_t objId = *(uint32_t*)luaL_checkudata(L, 1, "GameObject");
		GameObject* obj = GameObject::GetObjectById(objId);
		const char* key = luaL_checkstring(L, 2);

		if (!obj)
		{
			/*
				TODO 14/09/2024
				Back in Roblox, the Engine seems to preserve the state of the Instance upon it's destruction
				Attempting to assign to a `:Destroy`'d object still works, but it's
				`Parent` cannot be changed.
				This is what I'd like to have, but right now I'll just make every member
				be `nil` so it's easy for scripts to recognize an invalid/invalidated GameObject
				(I separated those two, because either the Object may not have been valid
				in the first place, or it was valid, but got deleted. But that's pedantic and pointless to
				differentiate for a proper API.)

				20/09/2024:
				It looks like `lua_newuserdatadtor` exists, refcount should be track-able with it
				so Objects are not de-alloc'd until refcount == 0
			*/
			lua_pushnil(L);
			return 1;
		}

		if (obj->HasProperty(key))
			ScriptEngine::L::PushGenericValue(L, obj->GetPropertyValue(key));
		else if (obj->HasFunction(key))
			ScriptEngine::L::PushFunction(L, key);
		else
		{
			GameObject* child = obj->GetChild(key);

			if (child)
				ScriptEngine::L::PushGameObject(L, child);
			else
			{
				std::string fullname = obj->GetFullName();

				luaL_error(L, std::vformat(
					"'{}' was neither a Member nor Child of {}",
					std::make_format_args(key, fullname)).c_str()
				);

				return 0;
			}
		}

		return 1;
	};

static auto api_gameobjnewindex = [](lua_State* L)
	{
		uint32_t objId = *(uint32_t*)luaL_checkudata(L, 1, "GameObject");
		GameObject* obj = GameObject::GetObjectById(objId);
		const char* key = luaL_checkstring(L, 2);

		// Refer the `!obj` clause in `api_gameobjindex`
		if (!obj)
		{
			// Uh
			// Idk
			// Just give an `attempt to index nil`
			lua_pushnil(L);
			lua_setfield(L, -1, key);

			return 0;
		}

		if (obj->HasProperty(key))
		{
			auto& prop = obj->GetProperty(key);

			const char* argAsString = luaL_tolstring(L, -1, NULL);
			const char* argTypeName = luaL_typename(L, -1);
			// 14/09/2024
			// Lua what the fuck
			// Please stop flip-flopping between negative and positive indexes for
			// stack offsets! What is the point! Why?!
			lua_Type argType = (lua_Type)lua_type(L, 3);
			
			if (!prop.Set)
				luaL_error(L, std::vformat(
					"Cannot set Property {}::{} to {} because it is read-only",
					std::make_format_args(obj->ClassName, key, argAsString)
				).c_str());

			auto reflToLuaIt = ScriptEngine::ReflectedTypeLuaEquivalent.find(prop.Type);

			if (reflToLuaIt == ScriptEngine::ReflectedTypeLuaEquivalent.end())
			{
				const std::string& typeName = Reflection::TypeAsString(prop.Type);
				luaL_error(L, std::vformat(
					"No defined mapping between Reflection::ValueType::{} and a Lua type",
					std::make_format_args(typeName)
				).c_str());
			}

			lua_Type desiredType = reflToLuaIt->second;

			if (desiredType != argType && (desiredType != LUA_TUSERDATA && argType != LUA_TNIL))
			{
				const char* desiredTypeName = lua_typename(L, (int)desiredType);
				luaL_error(L, std::vformat(
					"Expected type {} for member {}::{}, got {} instead",
					std::make_format_args(desiredTypeName, obj->ClassName, key, argTypeName)
				).c_str());
			}
			else
			{
				lua_pushvalue(L, 3);

				bool wasSuccessful = true;
				Reflection::GenericValue newValue = ScriptEngine::L::LuaValueToGeneric(L);

				if (wasSuccessful)
					obj->SetPropertyValue(key, newValue);
			}
		}
		else
		{
			luaL_error(L, (std::vformat(
				"Attempt to set invalid Member '{}' of GameObject",
				std::make_format_args(key)
			)).c_str());
		}

		return 0;
	};

static auto api_gameobjecttostring = [](lua_State* L)
	{
		uint32_t objId = *(uint32_t*)luaL_checkudata(L, 1, "GameObject");
		GameObject* object = GameObject::GetObjectById(objId);
		
		if (object)
			lua_pushstring(L, object->GetFullName().c_str());
		else
			lua_pushnil(L);

		return 1;
	};

static auto api_newvec3 = [](lua_State* L)
	{
		double x = luaL_checknumber(L, 1);
		double y = luaL_checknumber(L, 2);
		double z = luaL_checknumber(L, 3);

		ScriptEngine::L::PushGenericValue(L, Vector3(x, y, z).ToGenericValue());

		return 1;
	};

static auto api_vec3index = [](lua_State* L)
	{
		Vector3* vec = (Vector3*)luaL_checkudata(L, 1, "Vector3");
		const char* key = luaL_checkstring(L, 2);

		lua_getglobal(L, "Vector3");
		lua_pushstring(L, key);
		lua_rawget(L, -2);

		// Pass-through to Vector3.new
		if (!lua_isnil(L, -1))
			return 1;

		if (strcmp(key, "X") == 0)
		{
			lua_pushnumber(L, vec->X);
			return 1;
		}
		else if (strcmp(key, "Y") == 0)
		{
			lua_pushnumber(L, vec->Y);
			return 1;
		}
		else if (strcmp(key, "Z") == 0)
		{
			lua_pushnumber(L, vec->Z);
			return 1;
		}
		else if (strcmp(key, "Magnitude") == 0)
		{
			lua_pushnumber(L, vec->Magnitude);
			return 1;
		}
		else
			luaL_error(L, "Invalid key %s", key);

		//if (vec->HasProperty(key))
		//{
		//	Reflection::GenericValue value = vec->GetPropertyValue(key);
		//	pushGenericValue(L, value);

		//	return 1;
		//}
		//else if (vec->HasFunction(key))
		//{
		//	//pushFunction<Vector3>(L, vec, key);

		//	//return 1;

		//	return 0;
		//}
		//else
		//{
		//	luaL_error(L, std::vformat(
		//		"{} is not a valid member of Vector3",
		//		std::make_format_args(key)
		//	).c_str());
		//}
	};

static auto api_vec3newindex = [](lua_State* L)
	{
		luaL_error(L, "Vector3s are immutable");
	};

static auto api_vec3tostring = [](lua_State* L)
	{
		Vector3 vec = *(Vector3*)luaL_checkudata(L, 1, "Vector3");
		lua_pushstring(L, vec.ToString().c_str());

		return 1;
	};

static void* l_alloc(void*, void* ptr, size_t, size_t nsize)
{
	if (nsize == 0) {
		free(ptr);
		return NULL;
	}
	else
		return realloc(ptr, nsize);
}

static void initDefaultState()
{
	DefaultState = lua_newstate(l_alloc, nullptr);
	// Load Standard Library ('print' etc)
	// TODO: `require` is NOT part of the STL, copy
	// it from Luau REPL (`lua_require`)
	luaL_openlibs(DefaultState);

	// Vector3
	{
		lua_newtable(DefaultState);

		lua_pushcfunction(DefaultState, api_newvec3, "Vector3.new");
		lua_setfield(DefaultState, -2, "new");

		lua_setglobal(DefaultState, "Vector3");

		luaL_newmetatable(DefaultState, "Vector3");

		lua_pushcfunction(DefaultState, api_vec3index, "Vector3.__index");
		lua_setfield(DefaultState, -2, "__index");

		//lua_pushcfunction(DefaultState, api_vec3newindex, "Vector3.__newindex");
		//lua_setfield(DefaultState, -2, "__newindex");

		lua_pushcfunction(DefaultState, api_vec3tostring, "Vector3.__tostring");
		lua_setfield(DefaultState, -2, "__tostring");

		lua_pushstring(DefaultState, "Vector3");
		lua_setfield(DefaultState, -2, "__type");
	}

	// Matrix 
	{
		lua_newtable(DefaultState);

		lua_pushcfunction(
			DefaultState,
			[](lua_State* L)
			{
				ScriptEngine::L::PushGenericValue(L, glm::mat4(1.f));
				return 1;
			},
			"Matrix.new"
		);
		lua_setfield(DefaultState, -2, "new");

		lua_pushcfunction(
			DefaultState,
			[](lua_State* L)
			{
				float x = static_cast<float>(luaL_checknumber(L, 1));
				float y = static_cast<float>(luaL_checknumber(L, 2));
				float z = static_cast<float>(luaL_checknumber(L, 3));

				glm::mat4 t(1.f);
				t[3][0] = x;
				t[3][1] = y;
				t[3][2] = z;

				ScriptEngine::L::PushGenericValue(L, t);

				return 1;
			},
			"Matrix.fromTranslation"
		);
		lua_setfield(DefaultState, -2, "fromTranslation");

		lua_pushcfunction(
			DefaultState,
			[](lua_State* L)
			{
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
		lua_setfield(DefaultState, -2, "fromEulerAnglesXYZ");

		lua_pushcfunction(
			DefaultState,
			[](lua_State* L)
			{
				Vector3& a = *(Vector3*)luaL_checkudata(L, 1, "Vector3");
				Vector3& b = *(Vector3*)luaL_checkudata(L, 2, "Vector3");

				ScriptEngine::L::PushGenericValue(
					L,
					glm::lookAt((glm::vec3)a, (glm::vec3)b, glm::vec3(0.f, 1.f, 0.f))
				);

				return 1;
			},
			"Matrix.lookAt"
		);
		lua_setfield(DefaultState, -2, "lookAt");

		lua_setglobal(DefaultState, "Matrix");

		luaL_newmetatable(DefaultState, "Matrix");

		lua_pushstring(DefaultState, "Matrix");
		lua_setfield(DefaultState, -2, "__type");

		lua_pushcfunction(
			DefaultState,
			[](lua_State* L)
			{
				glm::mat4& a = *(glm::mat4*)luaL_checkudata(L, 1, "Matrix");
				glm::mat4& b = *(glm::mat4*)luaL_checkudata(L, 2, "Matrix");

				glm::mat4 result = a * b;

				ScriptEngine::L::PushGenericValue(L, result);

				return 1;
			},
			"Matrix.__mul"
		);
		lua_setfield(DefaultState, -2, "__mul");
	}

	// GameObject
	{
		lua_newtable(DefaultState);

		lua_pushcfunction(DefaultState, api_newobject, "GameObject.new");
		lua_setfield(DefaultState, -2, "new");

		lua_setglobal(DefaultState, "GameObject");

		luaL_newmetatable(DefaultState, "GameObject");

		lua_pushcfunction(DefaultState, api_gameobjindex, "GameObject.__index");
		lua_setfield(DefaultState, -2, "__index");

		lua_pushcfunction(DefaultState, api_gameobjnewindex, "GameObject.__newindex");
		lua_setfield(DefaultState, -2, "__newindex");

		lua_pushcfunction(DefaultState, api_gameobjecttostring, "GameObject.__tostring");
		lua_setfield(DefaultState, -2, "__tostring");

		lua_pushstring(DefaultState, "GameObject");
		lua_setfield(DefaultState, -2, "__type");
	}

	ScriptEngine::L::PushGameObject(DefaultState, GameObject::s_DataModel);
	lua_setglobal(DefaultState, "game");

	ScriptEngine::L::PushGameObject(DefaultState, GameObject::s_DataModel->GetChild("Workspace"));
	lua_setglobal(DefaultState, "workspace");

	for (auto& pair : ScriptEngine::L::GlobalFunctions)
	{
		lua_CFunction func = pair.second;

		lua_pushlightuserdata(DefaultState, func);

		lua_pushcclosure(
			DefaultState,
			[](lua_State* L)
			{
				lua_CFunction func = (lua_CFunction)lua_touserdata(L, lua_upvalueindex(1));

				try
				{
					return func(L);
				}
				catch (std::string e)
				{
					luaL_error(L, e.c_str());
				}
				catch (const char* e)
				{
					luaL_error(L, e);
				}

			},
			pair.first.c_str(),
			1
		);
		lua_setglobal(DefaultState, pair.first.c_str());
	}
}

void Object_Script::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_DECLAREPROP(
		"SourceFile",
		String,
		[](GameObject* p)
		{
			return Reflection::GenericValue(dynamic_cast<Object_Script*>(p)->SourceFile);
		},
		[](GameObject* p, Reflection::GenericValue newval)
		{
			Object_Script* scr = dynamic_cast<Object_Script*>(p);
			scr->LoadScript(newval.AsString());
		}
	);

	//REFLECTION_DECLAREPROP_SIMPLE(Object_Script, SourceFile, String);
	REFLECTION_DECLAREFUNC(
		"Reload",
		{},
		{ Reflection::ValueType::Bool },
		[](GameObject* p, const Reflection::GenericValue&)
		-> std::vector<Reflection::GenericValue>
		{
			Object_Script* scr = dynamic_cast<Object_Script*>(p);

			bool reloadSuccess = scr->Reload();
			return { reloadSuccess };
		}
	);

	REFLECTION_INHERITAPI(GameObject);
}

Object_Script::Object_Script()
{
	this->Name = "Script";
	this->ClassName = "Script";
	m_Bytecode = NULL;

	if (!DefaultState)
		initDefaultState();

	// `L` is initialized in Object_Script::Reload
	m_L = nullptr;

	s_DeclareReflections();
}

void Object_Script::Initialize()
{
	//this->Reload();
}

void Object_Script::Update(double dt)
{
	s_WindowGrabMouse = ScriptEngine::s_BackendScriptWantGrabMouse;

	// The first Script to be updated in the current frame will
	// need to handle resuming scheduled (i.e. yielded-but-now-hopefully-finished)
	// coroutines, the poor bastard
	// 23/09/2024
	for (auto & pair : ScriptEngine::s_YieldedCoroutines)
	{
		lua_State* coroutine = pair.first;
		std::shared_future<Reflection::GenericValue>& future = pair.second;

		if (future.valid())
		{
			// 23/09/2024 TODO This is just sad
			std::future_status status = pair.second.wait_for(std::chrono::seconds(0));
			if (status != std::future_status::ready)
				continue;

			// what the function returned
			const Reflection::GenericValue& retval = future.get();

			ScriptEngine::L::PushGenericValue(coroutine, retval);

			lua_Status resumeStatus = (lua_Status)lua_resume(coroutine, coroutine, 1);

			if (resumeStatus != LUA_OK && resumeStatus != LUA_YIELD)
			{
				const char* errstr = lua_tostring(coroutine, -1);
				std::string fullname = this->GetFullName();

				Debug::Log(std::vformat(
					"Luau yielded-then-resumed error: {}",
					std::make_format_args(errstr)
				));
			}


			ScriptEngine::s_YieldedCoroutines.erase(coroutine);
		}
	}

	if (ScriptEngine::s_YieldedCoroutines.find(m_L) != ScriptEngine::s_YieldedCoroutines.end())
		return;
	// We don't `::Reload` when the Script is being yielded,
	// because, what the hell will happen when it's resumed by the `for`-loop
	// above? Will it create a "ghost" Script? Not good. 23/09/2024
	if (!m_L || m_StaleSource)
		this->Reload();

	lua_getglobal(m_L, "Update");

	if (!lua_isfunction(m_L, -1))
		m_HasUpdate = false;
	else
	{
		lua_pushnumber(m_L, dt);
		// why do all of these functions say they return `int` and not
		// `lua_Status` like they actually do?? 23/09/2024
		lua_Status updateStatus = (lua_Status)lua_pcall(m_L, 1, 0, 0);

		if (updateStatus != LUA_OK && updateStatus != LUA_YIELD)
		{
			m_HasUpdate = false;

			const char* errstr = lua_tostring(m_L, -1);
			std::string fullname = this->GetFullName();

			Debug::Log(std::vformat(
				"Luau runtime error: {}",
				std::make_format_args(errstr)
			));
		}
	}
}

bool Object_Script::LoadScript(std::string const& scriptFile)
{
	if (SourceFile == scriptFile)
		return true;

	this->SourceFile = scriptFile;
	m_StaleSource = true;

	// TODO 14/09/2024
	// Don't want the script to run before SceneFormat has fully
	// deserialized the scene and the `script` global to
	// report it is parented to `nil`. Only want it to run when
	// parented under the `DataModel`. An `::IsDescendantOf` would be
	// ideal, but it's not needed right now as this value will always
	// be set before Script get's its GameObject properties assigned
	// (i.e. it's Parent)
	// This is in this property setter so that the Script can be
	// manually loaded if the need every arises
	if (!this->GetParent())
		return false;
	else
		return this->Reload();
}

bool Object_Script::Reload()
{
	bool fileExists = true;
	m_Source = FileRW::ReadFile(SourceFile, &fileExists);

	m_StaleSource = false;

	if (!fileExists)
	{
		Debug::Log(
			std::vformat(
				"Script '{}' references invalid Source File '{}'!",
				std::make_format_args(this->Name, this->SourceFile
				)
			)
		);

		return false;
	}

	if (m_L)
		lua_resetthread(m_L);
	else
		m_L = lua_newthread(DefaultState);

	ScriptEngine::L::PushGameObject(m_L, this);
	lua_setglobal(m_L, "script");

	std::string FullName = this->GetFullName();
	std::string chunkname = std::vformat("{}", std::make_format_args(FullName));

	// FROM: https://github.com/luau-lang/luau/ README

	// needs lua.h and luacode.h
	size_t bytecodeSize = 0;
	// Keeps crashing upon reloading the script twice, maybe
	// `malloc` some so it isn't a nullptr (due to the `free`)
	m_Bytecode = (char*)malloc(4);
	m_Bytecode = luau_compile(m_Source.c_str(), m_Source.length(), NULL, &bytecodeSize);

	int result = luau_load(m_L, chunkname.c_str(), m_Bytecode, bytecodeSize, 0);
	
	free(m_Bytecode);

	if (result == 0)
	{
		// Run the script

		lua_Status resumeResult = (lua_Status)lua_resume(m_L, nullptr, 0);

		if (resumeResult == lua_Status::LUA_OK)
		{
			lua_getglobal(m_L, "Update");

			if (lua_isfunction(m_L, -1))
				m_HasUpdate = true;
		}

		else if (resumeResult != lua_Status::LUA_YIELD)
		{
			const char* errstr = lua_tostring(m_L, -1);

			Debug::Log(std::vformat(
				"Luau script init error: {}",
				std::make_format_args(errstr)
			));
		}

		return true; /* return chunk main function */
	}
	else
	{
		int topidx = lua_gettop(m_L);
		const char* errstr = lua_tostring(m_L, topidx);

		Debug::Log(std::vformat("Luau compile error {}: {}: '{}'", std::make_format_args(result, this->Name, errstr)));
		return false;
	}
}
