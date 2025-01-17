#include <glm/gtc/matrix_transform.hpp>
#include <luau/VM/include/lualib.h>
#include <tracy/Tracy.hpp>

#include "gameobject/Script.hpp"
#include "datatype/GameObject.hpp"
#include "datatype/Vector3.hpp"
#include "datatype/Color.hpp"
#include "UserInput.hpp"
#include "FileRW.hpp"
#include "Log.hpp"
#include "gameobject/ScriptEngine.hpp"

#define LUA_ASSERT(res, err, ...) if (!res) { luaL_error(L, err, __VA_ARGS__); }

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Script);

static bool s_DidInitReflection = false;

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

		LUA_ASSERT(obj, "Tried to index '%s' of a deleted Game Object", key);

		if (obj->HasProperty(key))
			ScriptEngine::L::PushGenericValue(L, obj->GetPropertyValue(key));

		else if (obj->HasFunction(key))
			ScriptEngine::L::PushFunction(L, key);

		else
		{
			GameObject* child = obj->FindChild(key);

			if (child)
				ScriptEngine::L::PushGameObject(L, child);
			else
				// explicitly pushnil so the error message doesn't say
				// "attempt to call a GameObject value"
				// 10/01/2025
				lua_pushnil(L);
		}

		return 1;
	};

static auto api_gameobjnewindex = [](lua_State* L)
	{
		ZoneScopedNC("GameObject.__newindex", tracy::Color::LightSkyBlue);

		GameObject* obj = GameObject::FromGenericValue(ScriptEngine::L::LuaValueToGeneric(L, 1));
		const char* key = luaL_checkstring(L, 2);

		ZoneText(key, strlen(key));

		LUA_ASSERT(strcmp(key, "Exists") != 0, "'Exists' is read-only! - 21/12/2024");

		LUA_ASSERT(obj, "Tried to assign to the '%s' of a deleted Game Object", key);

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

				Reflection::GenericValue newValue = ScriptEngine::L::LuaValueToGeneric(L);

				try
				{
					obj->SetPropertyValue(key, newValue);
				}
				catch (std::string err)
				{
					luaL_errorL(L, err.c_str());
				}
			}
		}
		else
		{
			std::string fullname = obj->GetFullName();
			luaL_error(L, (std::vformat(
				"Attempt to set invalid Member '{}' of {} '{}'",
				std::make_format_args(key, obj->ClassName, fullname)
			)).c_str());
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
		ZoneScopedNC("Vector3.__index", tracy::Color::LightSkyBlue);

		Vector3* vec = (Vector3*)luaL_checkudata(L, 1, "Vector3");
		const char* key = luaL_checkstring(L, 2);

		ZoneText(key, strlen(key));

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
			lua_pushnumber(L, vec->Magnitude());
			return 1;
		}
		else if (strcmp(key, "Normalized") == 0)
		{
			ScriptEngine::L::PushGenericValue(L, (*vec / vec->Magnitude()).ToGenericValue());
			return 1;
		}
		else if (strcmp(key, "Dot") == 0)
		{
			ScriptEngine::L::PushGenericValue(L, vec->ToGenericValue());

			lua_pushcclosure(
				L,
				[](lua_State* L)
				-> int
				{
					Vector3* v1 = (Vector3*)luaL_checkudata(L, lua_upvalueindex(1), "Vector3");
					Vector3* v2 = (Vector3*)luaL_checkudata(L, 1, "Vector3");

					ScriptEngine::L::PushGenericValue(L, v1->Dot(*v2));
					
					return 1;
				},
				"Vector3::Dot",
				1
			);

			return 1;
		}
		else if (strcmp(key, "Cross") == 0)
		{
			ScriptEngine::L::PushGenericValue(L, vec->ToGenericValue());

			lua_pushcclosure(
				L,
				[](lua_State* L)
				-> int
				{
					Vector3* v1 = (Vector3*)luaL_checkudata(L, lua_upvalueindex(1), "Vector3");
					Vector3* v2 = (Vector3*)luaL_checkudata(L, 1, "Vector3");

					ScriptEngine::L::PushGenericValue(L, v1->Cross(*v2).ToGenericValue());

					return 1;
				},
				"Vector3::Cross",
				1
			);

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
			luaL_error(L, "Invalid key %s", key);
	};

static auto api_coltostring = [](lua_State* L)
	{
		Color* col = (Color*)luaL_checkudata(L, 1, "Color");

		lua_pushstring(
			L,
			std::vformat(
				"{}, {}, {}",
				std::make_format_args(col->R, col->G, col->B)
			).c_str()
		);

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

static lua_State* createState()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	lua_State* state = lua_newstate(l_alloc, nullptr);
	// Load Standard Library ('print' etc)
	// TODO: `require` is NOT part of the STL, copy
	// it from Luau REPL (`lua_require`)
	luaL_openlibs(state);
	luaopen_vector(state);

	lua_pushcfunction(
		state,
		[](lua_State* L)
		{
			// FROM:
			// `luaB_print`
			// `Luau/VM/src/lbaselib.cpp`
			// 11/11/2024

			int n = lua_gettop(L); // number of arguments
			for (int i = 1; i <= n; i++)
			{
				size_t l;
				const char* s = luaL_tolstring(L, i, &l); // convert to string using __tostring et al

				if (i > 1)
					Log::Append(" &&");
				else
					Log::Info("&&");

				Log::Append(std::string(s) + "&&");
				lua_pop(L, 1); // pop result
			}
			Log::Append("\n&&");

			return 0;
		},
		"PhxPrintOverride"
	);
	lua_setglobal(state, "print");

	// Vector3
	{
		lua_newtable(state);

		lua_pushcfunction(state, api_newvec3, "Vector3.new");
		lua_setfield(state, -2, "new");

		lua_setglobal(state, "Vector3");

		luaL_newmetatable(state, "Vector3");

		lua_pushcfunction(state, api_vec3index, "Vector3.__index");
		lua_setfield(state, -2, "__index");

		//lua_pushcfunction(state, api_vec3newindex, "Vector3.__newindex");
		//lua_setfield(state, -2, "__newindex");

		lua_pushcfunction(state, api_vec3tostring, "Vector3.__tostring");
		lua_setfield(state, -2, "__tostring");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("Vector3.__add", tracy::Color::LightSkyBlue);

				Vector3 a = Vector3(ScriptEngine::L::LuaValueToGeneric(L, -2));
				Vector3 b = Vector3(ScriptEngine::L::LuaValueToGeneric(L, -1));

				ScriptEngine::L::PushGenericValue(L, (a + b).ToGenericValue());

				return 1;
			},
			"Vector3.__add"
		);
		lua_setfield(state, -2, "__add");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("Vector3.__sub", tracy::Color::LightSkyBlue);

				Vector3 a = Vector3(ScriptEngine::L::LuaValueToGeneric(L, -2));
				Vector3 b = Vector3(ScriptEngine::L::LuaValueToGeneric(L, -1));

				ScriptEngine::L::PushGenericValue(L, (a - b).ToGenericValue());

				return 1;
			},
			"Vector3.__sub"
		);
		lua_setfield(state, -2, "__sub");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("Vector3.__mul", tracy::Color::LightSkyBlue);

				Vector3 a = Vector3(ScriptEngine::L::LuaValueToGeneric(L, -2));

				int bt = lua_type(L, 2);

				if (bt == LUA_TNUMBER)
				{
					double b = luaL_checknumber(L, 2);

					ScriptEngine::L::PushGenericValue(L, (a * b).ToGenericValue());

					return 1;
				}
				else if (bt == LUA_TUSERDATA)
				{
					Vector3 b = ScriptEngine::L::LuaValueToGeneric(L, -1);

					ScriptEngine::L::PushGenericValue(L, (a * b).ToGenericValue());

					return 1;
				}

				luaL_errorL(L, "Expected multiplication against Vector3 or number");
			},
			"Vector3.__mul"
		);
		lua_setfield(state, -2, "__mul");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("Vector3.__div", tracy::Color::LightSkyBlue);

				Vector3 a = Vector3(ScriptEngine::L::LuaValueToGeneric(L, -2));

				int bt = lua_type(L, 2);

				if (bt == LUA_TNUMBER)
				{
					double b = luaL_checknumber(L, 2);

					ScriptEngine::L::PushGenericValue(L, (a / b).ToGenericValue());

					return 1;
				}
				else if (bt == LUA_TUSERDATA)
				{
					Vector3 b = ScriptEngine::L::LuaValueToGeneric(L, -1);

					ScriptEngine::L::PushGenericValue(L, Vector3(a.X / b.X, a.Y / b.Y, a.Z / b.Z).ToGenericValue());

					return 1;
				}

				luaL_errorL(L, "Expected division against Vector3 or number");
			},
			"Vector3.__div"
		);
		lua_setfield(state, -2, "__div");

		lua_pushstring(state, "Vector3");
		lua_setfield(state, -2, "__type");
	}

	// Color
	{
		lua_newtable(state);

		lua_pushcfunction(state, api_newcol, "Color.new");
		lua_setfield(state, -2, "new");

		lua_setglobal(state, "Color");

		luaL_newmetatable(state, "Color");

		lua_pushcfunction(state, api_colindex, "Color.__index");
		lua_setfield(state, -2, "__index");

		//lua_pushcfunction(state, api_vec3newindex, "Vector3.__newindex");
		//lua_setfield(state, -2, "__newindex");

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
				case (1):
				{
					Vector3& vec = *(Vector3*)luaL_checkudata(L, -1, "Vector3");
					m[3] = glm::vec4((glm::vec3)vec, 1.f);

					break;
				}
				case (3):
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
						Vector3(glm::vec3(m[3])).ToGenericValue()
					);
				else if (strcmp(k, "Forward") == 0)
					ScriptEngine::L::PushGenericValue(
						L,
						Vector3(glm::normalize(glm::vec3(m[2]))).ToGenericValue()
					);
				else if (strcmp(k, "Up") == 0)
					ScriptEngine::L::PushGenericValue(
						L,
						Vector3(glm::normalize(glm::vec3(m[1]))).ToGenericValue()
					);
				else if (strcmp(k, "Right") == 0)
					ScriptEngine::L::PushGenericValue(
						L,
						Vector3(glm::normalize(glm::vec3(m[0]))).ToGenericValue()
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

				int nargs = lua_gettop(L) - 1;

				GameObject* g = GameObject::FromGenericValue(ScriptEngine::L::LuaValueToGeneric(L, 1));
				const char* k = L->namecall->data; // this is weird 10/01/2025

				ZoneText(k, strlen(k));

				int numresults = 0;

				try
				{
					numresults = ScriptEngine::L::HandleFunctionCall(
						L,
						g,
						k,
						nargs
					);
				}
				catch (std::string err)
				{
					luaL_errorL(L, err.c_str());
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

	ScriptEngine::L::PushGameObject(state, GameObject::s_DataModel);
	lua_setglobal(state, "game");

	ScriptEngine::L::PushGameObject(state, GameObject::s_DataModel->FindChild("Workspace"));
	lua_setglobal(state, "workspace");

	for (auto& pair : ScriptEngine::L::GlobalFunctions)
	{
		lua_CFunction func = pair.second;

		lua_pushlightuserdata(state, func);
		lua_pushstring(state, pair.first.c_str());
		
		lua_pushcclosure(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("xStandardLibrary", tracy::Color::LightSkyBlue);

				const char* fnName = lua_tostring(L, lua_upvalueindex(2));

				ZoneText(fnName, strlen(fnName));

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
			2
		);
		lua_setglobal(state, pair.first.c_str());
	}

	return state;
}

void Object_Script::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROP(
		"SourceFile",
		String,
		[](Reflection::Reflectable* p)
		{
			return Reflection::GenericValue(static_cast<Object_Script*>(p)->SourceFile);
		},
		[](Reflection::Reflectable* p, Reflection::GenericValue newval)
		{
			Object_Script* scr = static_cast<Object_Script*>(p);
			scr->LoadScript(newval.AsString());
		}
	);

	//REFLECTION_DECLAREPROP_SIMPLE(Object_Script, SourceFile, String);
	REFLECTION_DECLAREFUNC(
		"Reload",
		{},
		{ Reflection::ValueType::Bool },
		[](Reflection::Reflectable* p, const Reflection::GenericValue&)
		-> std::vector<Reflection::GenericValue>
		{
			Object_Script* scr = static_cast<Object_Script*>(p);

			bool reloadSuccess = scr->Reload();
			return { reloadSuccess };
		}
	);
}

Object_Script::Object_Script()
{
	this->Name = "Script";
	this->ClassName = "Script";

	// `L` is initialized in Object_Script::Reload
	m_L = nullptr;

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

void Object_Script::Initialize()
{
	//this->Reload();
}

static void resumeScheduledCoroutines()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	for (size_t corIdx = 0; corIdx < ScriptEngine::s_YieldedCoroutines.size(); corIdx++)
	{
		const ScriptEngine::YieldedCoroutine& corInfo = ScriptEngine::s_YieldedCoroutines.at(corIdx);
		lua_State* coroutine = corInfo.Coroutine;
		const std::shared_future<Reflection::GenericValue>& future = corInfo.Future;

		if (future.valid())
		{
			// 23/09/2024 TODO This is just sad
			std::future_status status = future.wait_for(std::chrono::seconds(0));
			if (status != std::future_status::ready)
				continue;

			// what the function returned
			Reflection::GenericValue retval = future.get();

			ScriptEngine::L::PushGenericValue(coroutine, retval);

			lua_Status resumeStatus = (lua_Status)lua_resume(coroutine, nullptr, 1);

			if (resumeStatus != LUA_OK && resumeStatus != LUA_YIELD)
			{
				const char* errstr = lua_tostring(coroutine, -1);

				Log::Error(std::vformat(
					"Luau yielded-then-resumed error: {}",
					std::make_format_args(errstr)
				));
			}

			ScriptEngine::s_YieldedCoroutines.erase(ScriptEngine::s_YieldedCoroutines.begin() + corIdx);
			//lua_unref(lua_mainthread(coroutine), corInfo.CoroutineReference);

			// the indexes of the coroutines will have changed, and `corIdx` will point
			// out-of-bounds. this is a lazy workaround. 24/12/2024
			// just fucking give up
			return;
		}
	}
}

void Object_Script::Update(double dt)
{
	s_WindowGrabMouse = ScriptEngine::s_BackendScriptWantGrabMouse;

	// The first Script to be updated in the current frame will
	// need to handle resuming scheduled (i.e. yielded-but-now-hopefully-finished)
	// coroutines, the poor bastard
	// 23/09/2024
	resumeScheduledCoroutines();

	ZoneScopedC(tracy::Color::LightSkyBlue);

	std::string fullname = this->GetFullName();
	ZoneTextF("Script: %s\nFile: %s", fullname.c_str(), this->SourceFile.c_str());

	if (m_StaleSource)
		this->Reload();

	// script has failed to load (compilation error) 07/11/2024
	if (!m_L)
		return;

	if (lua_status(m_L) != LUA_OK)
		return;

	lua_getglobal(m_L, "Update");

	if (lua_isfunction(m_L, -1))
	{
		lua_pushnumber(m_L, dt);

		// why do all of these functions say they return `int` and not
		// `lua_Status` like they actually do?? 23/09/2024
		lua_Status updateStatus = (lua_Status)lua_pcall(m_L, 1, 0, 0);

		if (updateStatus != LUA_OK && updateStatus != LUA_YIELD)
		{
			const char* errstr = lua_tostring(m_L, -1);

			Log::Error(std::vformat(
				"Luau runtime error: {}",
				std::make_format_args(errstr)
			));

			lua_close(m_L);
			m_L = nullptr;
		}
	}
	else
		lua_pop(m_L, 1);
}

bool Object_Script::LoadScript(const std::string& scriptFile)
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

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
	ZoneScopedC(tracy::Color::LightSkyBlue);

	std::string fullName = this->GetFullName();

	ZoneTextF("Script: %s\nFile: %s", fullName.c_str(), this->SourceFile.c_str());

	bool fileExists = true;
	m_Source = FileRW::ReadFile(SourceFile, &fileExists);

	m_StaleSource = false;
	
	// `resetthread` doesn't get rid of globals btw
	// new things learned every day
	// 24/12/2024
	if (m_L)
		lua_close(m_L);
	m_L = createState();

	if (!fileExists)
	{
		Log::Error(
			std::vformat(
				"Script '{}' references invalid Source File '{}'!",
				std::make_format_args(fullName, this->SourceFile)
			)
		);

		return false;
	}

	ScriptEngine::L::PushGameObject(m_L, this);
	lua_setglobal(m_L, "script");

	std::string chunkname = std::vformat("{}", std::make_format_args(fullName));

	// FROM: https://github.com/luau-lang/luau/ README

	// needs lua.h and luacode.h
	size_t bytecodeSize = 0;
	// Keeps crashing upon reloading the script twice, maybe
	// `malloc` some so it isn't a nullptr (due to the `free`)
	char* bytecode = luau_compile(m_Source.c_str(), m_Source.length(), NULL, &bytecodeSize);

	int result = luau_load(m_L, chunkname.c_str(), bytecode, bytecodeSize, 0);
	
	if (result == 0)
	{
		// prevent ourselves from being deleted by the code we run.
		// if that code errors, it'll be a use-after-free as we try
		// to access `m_L` to get the error message
		// 24/12/2024
		GameObjectRef<Object_Script> dontKillMePlease = this;

		lua_Status resumeResult = (lua_Status)lua_resume(m_L, m_L, 0);

		if (resumeResult != lua_Status::LUA_OK && resumeResult != lua_Status::LUA_YIELD)
		{
			const char* errStr = lua_tostring(m_L, -1);

			Log::Error(std::vformat(
				"Luau script init error: {}",
				std::make_format_args(errStr)
			));

			return false;
		}

		return true; /* return chunk main function */
	}
	else
	{
		const char* errstr = lua_tostring(m_L, -1);

		Log::Error(std::vformat(
			"Luau compile error {}: {}: '{}'",
			std::make_format_args(result, this->Name, errstr))
		);

		return false;
	}
}
