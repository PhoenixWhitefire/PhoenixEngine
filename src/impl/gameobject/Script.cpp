/*
	
	11/09/2024:
	This is a mess. If you see any references to `Reflection::Reflectable`,
	`Reflection::IProperty` or `Reflection::IFunction`, just translate them
	in your head to `GameObject`, `IProperty` and `IFunction`. They are different,
	and I have taken the approach of trying to reduce any pre-emptive abstractions

*/

#include<luau/Compiler/include/luacode.h>
#include<luau/VM/include/lualib.h>
#include<luau/Common/include/Luau/Common.h>
#include<glm/gtc/matrix_transform.hpp>

#include"gameobject/Script.hpp"
#include"datatype/GameObject.hpp"
#include"datatype/Vector3.hpp"
#include"UserInput.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

#define LUA_ASSERT(res, err) if (!res) { luaL_error(L, err); }

static RegisterDerivedObject<Object_Script> RegisterClassAs("Script");

static bool s_DidInitReflection = false;
static lua_State* DefaultState = nullptr;

static const std::unordered_map<Reflection::ValueType, lua_Type> ReflectionTypeToLuaType =
{
	{ Reflection::ValueType::Null, lua_Type::LUA_TNIL },
	{ Reflection::ValueType::Bool, lua_Type::LUA_TBOOLEAN },
	{ Reflection::ValueType::Double, lua_Type::LUA_TNUMBER },
	{ Reflection::ValueType::Integer, lua_Type::LUA_TNUMBER },
	{ Reflection::ValueType::String, lua_Type::LUA_TSTRING },
	{ Reflection::ValueType::Vector3, lua_Type::LUA_TUSERDATA },
	{ Reflection::ValueType::Matrix, lua_Type::LUA_TUSERDATA },
	{ Reflection::ValueType::GameObject, lua_Type::LUA_TUSERDATA },
	{ Reflection::ValueType::Array, lua_Type::LUA_TTABLE },
	{ Reflection::ValueType::Map, lua_Type::LUA_TTABLE }
};

// Returns a `Reflection::GenericValue` of the topmost value of the Luau stack
static Reflection::GenericValue luaTypeToGeneric(lua_State* L, int luaT, int stackIndex = -1)
{
	Reflection::GenericValue gv;

	switch (luaT)
	{
	case (lua_Type::LUA_TNIL):
	{
		gv.Type = Reflection::ValueType::Null;
		break;
	}
	case (lua_Type::LUA_TBOOLEAN):
	{
		gv = (bool)lua_toboolean(L, stackIndex);
		break;
	}
	case (lua_Type::LUA_TNUMBER):
	{
		gv = lua_tonumber(L, stackIndex);
		break;
	}
	case (lua_Type::LUA_TSTRING):
	{
		gv.String = lua_tostring(L, stackIndex);
		gv.Type = Reflection::ValueType::String;
		break;
	}
	case (lua_Type::LUA_TUSERDATA):
	{
		// IMPORTANT!!
		// Requires `LuauPreserveLudataRenaming` to be enabled in `Luau/VM/src/ltm.cpp`
		// 11/09/2024
		const char* tname = luaL_typename(L, stackIndex);
		
		if (strcmp(tname, "Vector3") == 0)
		{
			Vector3 vec = *(Vector3*)lua_touserdata(L, stackIndex);
			gv = vec.ToGenericValue();
		}
		else if (strcmp(tname, "Matrix") == 0)
		{
			gv = Reflection::GenericValue(*(glm::mat4*)lua_touserdata(L, stackIndex));
		}
		else if (strcmp(tname, "GameObject") == 0)
		{
			gv = *(uint32_t*)lua_touserdata(L, stackIndex);
			gv.Type = Reflection::ValueType::GameObject;
		}

		break;
	}
	case (lua_Type::LUA_TTABLE):
	{
		// 15/09/2024
		// TODO
		// Maps

		std::vector<Reflection::GenericValue> items;

		// https://www.lua.org/manual/5.1/manual.html#lua_next
		lua_pushnil(L);

		while (lua_next(L, stackIndex - 1) != 0)
		{
			luaL_checknumber(L, -2);
			items.push_back(luaTypeToGeneric(L, lua_type(L, -1), -1));
			lua_pop(L, 1);
		}

		gv = items;

		break;
	}
	default:
	{
		const char* tname = luaL_typename(L, luaT);
		luaL_error(L, std::vformat(
			"Could not convert type {} to a GenericValue (no conversion case)",
			std::make_format_args(tname)).c_str()
		);
	}
	}

	return gv;
}

static void pushVector3(lua_State* L, const Vector3& vec)
{
	void* ptrTovec = lua_newuserdata(L, sizeof(Vector3));
	*(Vector3*)ptrTovec = vec;

	luaL_getmetatable(L, "Vector3");
	lua_setmetatable(L, -2);
}

static void pushMatrix(lua_State* L, const glm::tmat4x4<float>& Matrix)
{
	void* ptrToMtx = lua_newuserdata(L, sizeof(Matrix));
	*(glm::mat4*)ptrToMtx = Matrix;

	luaL_getmetatable(L, "Matrix");
	lua_setmetatable(L, -2);
}

static void pushGameObject(lua_State* L, GameObject* obj)
{
	uint32_t* ptrToObj = (uint32_t*)lua_newuserdata(L, sizeof(uint32_t));

	*ptrToObj = obj->ObjectId;

	luaL_getmetatable(L, "GameObject");
	lua_setmetatable(L, -2);
}

static void pushGenericValue(lua_State* L, const Reflection::GenericValue& gv)
{
	switch (gv.Type)
	{
	case (Reflection::ValueType::Null):
	{
		lua_pushnil(L);
		break;
	}
	case (Reflection::ValueType::Bool):
	{
		lua_pushboolean(L, gv.AsBool());
		break;
	}
	case (Reflection::ValueType::Integer):
	{
		lua_pushinteger(L, static_cast<int32_t>(gv.AsInteger()));
		break;
	}
	case (Reflection::ValueType::Double):
	{
		lua_pushnumber(L, gv.AsDouble());
		break;
	}
	case (Reflection::ValueType::String):
	{
		lua_pushstring(L, gv.String.c_str());
		break;
	}
	case (Reflection::ValueType::Vector3):
	{
		pushVector3(L, gv);
		break;
	}
	case (Reflection::ValueType::Matrix):
	{
		pushMatrix(L, gv.AsMatrix());
		break;
	}
	case (Reflection::ValueType::GameObject):
	{
		pushGameObject(L, GameObject::GetObjectById(static_cast<uint32_t>(gv.AsInteger())));
		break;
	}
	case (Reflection::ValueType::Array):
	{
		lua_newtable(L);

		for (int index = 0; index < gv.Array.size(); index++)
		{
			lua_pushinteger(L, index);
			pushGenericValue(L, gv.Array[index]);
			lua_settable(L, -3);
		}

		break;
	}
	case (Reflection::ValueType::Map):
	{
		if (!(gv.Array.size() % 2 == 0))
			throw("GenericValue type was Map, but it does not have an even number of elements!");

		lua_newtable(L);

		for (int index = 0; index < gv.Array.size(); index++)
		{
			pushGenericValue(L, gv.Array[index]);

			if ((index + 1) % 2 == 0)
				lua_settable(L, -3);
		}

		break;
	}
	default:
	{
		std::string typeName = Reflection::TypeAsString(gv.Type);
		luaL_error(L, std::vformat(
			"Could not provide Luau the GenericValue with type {}",
			std::make_format_args(typeName)).c_str()
		);
	}
	}
}

// Push a Reflection::Function onto the stack
// Specified via a pointer to the `Reflectable`, and the function name
static void pushFunction(lua_State* L, const char* name)
{
	// upvalues
	lua_pushstring(L, name);

	lua_pushcclosure(
		L,

		[](lua_State* L)
		{
			// -1 because the object is passed in as the first argument
			// (also means we don't need to add the object as an upvalue)
			// 11/09/2024
			int numArgs = lua_gettop(L) - 1;

			GameObject* refl = GameObject::GetObjectById(*(uint32_t*)luaL_checkudata(L, 1, "GameObject"));
			const char* fname = lua_tostring(L, lua_upvalueindex(1));

			auto& func = refl->GetFunction(fname);
			const std::vector<Reflection::ValueType>& paramTypes = func.Inputs;

			int numParams = static_cast<int32_t>(paramTypes.size());

			if (numArgs != numParams)
			{
				std::string argsString;

				for (int arg = 1; arg <= numArgs; arg++)
					argsString += std::string(luaL_typename(L, 0 - (numArgs - arg))) + ", ";

				argsString = argsString.substr(0, argsString.size() - 2);

				luaL_error(L, std::vformat(
					"Function '{}' expected {} arguments, got {} instead: ({})",
					std::make_format_args(fname, numParams, numArgs, argsString)
				).c_str());

				return 0;
			}

			std::vector<Reflection::GenericValue> inputs;

			// This *entire* for-loop is just for handling input arguments
			for (int index = 0; index < paramTypes.size(); index++)
			{
				Reflection::ValueType paramType = paramTypes[index];

				// Ex: W/ 3 args:
				// 0 = -3
				// 1 = -2
				// 2 = -1
				// Simpler than I thought actually
				int argStackIndex = index - numParams;

				auto expectedLuaTypeIt = ReflectionTypeToLuaType.find(paramType);

				if (expectedLuaTypeIt == ReflectionTypeToLuaType.end())
					throw(std::vformat(
						"Couldn't find the equivalent of a Reflection::ValueType::{} in Lua",
						std::make_format_args(Reflection::TypeAsString(paramType))
					));

				int expectedLuaType = (int)expectedLuaTypeIt->second;
				int actualLuaType = lua_type(L, argStackIndex);

				if (actualLuaType != expectedLuaType)
				{
					const char* expectedName = lua_typename(L, expectedLuaType);
					const char* actualTypeName = luaL_typename(L, argStackIndex);
					const char* providedValue = luaL_tolstring(L, argStackIndex, NULL);

					// I get that shitty fucking ::vformat can't handle
					// a SINGULAR fucking parameter that isn't an lvalue,
					// but an `int`?? A literal fucking scalar??? What is this bullshit????
					int indexAsLuaIndex = index + 1;

					luaL_error(L, std::vformat(
						"Argument {} expected to be of type {}, but was {} ({}) instead",
						std::make_format_args(
							indexAsLuaIndex,
							expectedName,
							providedValue,
							actualTypeName
						)
					).c_str());

					return 0;
				}
				else
					inputs.push_back(luaTypeToGeneric(L, expectedLuaType, argStackIndex));
			}

			// Now, onto the *REAL* business...
			std::vector<Reflection::GenericValue> outputs;

			try
			{
				outputs = func.Func(refl, inputs);
			}
			catch (std::string err)
			{
				luaL_error(L, err.c_str());
			}
			catch (const char* err)
			{
				luaL_error(L, err);
			}

			for (const Reflection::GenericValue& output : outputs)
				pushGenericValue(L, output);

			return (int)func.Outputs.size();

			// ... kinda expected more, but ngl i feel SOOOO gigabrain for
			// giving ::GenericValue an Array, like, it all just clicks in now!
			// And then Maps just being Arrays, except odd elements are the keys
			// and even elements are the values?! Call me Einstein already on god-
			// (Me writing this as Rendering is completely busted and I have no clue
			// why oh no
			// 15/08/2024
		},

		name,
		1
	);
}

static auto api_newobject = [](lua_State* L)
	{
		GameObject* newObject = GameObject::Create(luaL_checkstring(L, 1));
		pushGameObject(L, newObject);

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
		{
			Reflection::GenericValue value = obj->GetPropertyValue(key);
			pushGenericValue(L, value);
		}
		else if (obj->HasFunction(key))
		{
			pushFunction(L, key);
		}
		else
		{
			GameObject* child = obj->GetChild(key);

			if (child)
				pushGameObject(L, child);
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

			auto reflToLuaIt = ReflectionTypeToLuaType.find(prop.Type);

			if (reflToLuaIt == ReflectionTypeToLuaType.end())
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
				Reflection::GenericValue newValue = luaTypeToGeneric(L, desiredType);

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

		pushVector3(L, Vector3(x, y, z));

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
				pushMatrix(L, glm::mat4(1.f));
				return 1;
			},
			"Matrix.new"
		);
		lua_setfield(DefaultState, -2, "new");

		lua_pushcfunction(
			DefaultState,
			[](lua_State* L)
			{
				float x = luaL_checknumber(L, 1);
				float y = luaL_checknumber(L, 2);
				float z = luaL_checknumber(L, 3);

				glm::mat4 t(1.f);
				t[3][0] = x;
				t[3][1] = y;
				t[3][2] = z;

				pushMatrix(L, t);

				return 1;
			},
			"Matrix.fromTranslation"
		);
		lua_setfield(DefaultState, -2, "fromTranslation");

		lua_pushcfunction(
			DefaultState,
			[](lua_State* L)
			{
				float x = luaL_checknumber(L, 1);
				float y = luaL_checknumber(L, 2);
				float z = luaL_checknumber(L, 3);

				glm::mat4 t(1.f);
				t = glm::rotate(t, x, glm::vec3(1.f, 0.f, 0.f));
				t = glm::rotate(t, y, glm::vec3(0.f, 1.f, 0.f));
				t = glm::rotate(t, z, glm::vec3(0.f, 0.f, 1.f));

				pushMatrix(L, t);

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

				pushMatrix(L, glm::lookAt((glm::vec3)a, (glm::vec3)b, glm::vec3(0.f, 1.f, 0.f)));

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

				pushMatrix(L, result);

				return 1;
			},
			"Matrix.__mul"
		);
		lua_setfield(DefaultState, -2, "__mul");

		lua_pushcfunction(
			DefaultState,
			[](lua_State* L)
			{
				glm::mat4& mtx = *(glm::mat4*)luaL_checkudata(L, 1, "Matrix");
				int r = luaL_checkinteger(L, 2);
				int c = luaL_checkinteger(L, 3);

				lua_pushnumber(L, mtx[r][c]);

				return 1;
			},
			"matrix_getv"
		);
		lua_setglobal(DefaultState, "matrix_getv");

		lua_pushcfunction(
			DefaultState,
			[](lua_State* L)
			{
				glm::mat4& mtx = *(glm::mat4*)luaL_checkudata(L, 1, "Matrix");
				int r = luaL_checkinteger(L, 2);
				int c = luaL_checkinteger(L, 3);
				float v = static_cast<float>(luaL_checknumber(L, 4));

				mtx[r][c] = v;

				pushGenericValue(L, mtx);

				return 1;
			},
			"matrix_setv"
		);
		lua_setglobal(DefaultState, "matrix_setv");
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

	pushGameObject(DefaultState, GameObject::s_DataModel);
	lua_setglobal(DefaultState, "game");

	pushGameObject(DefaultState, GameObject::s_DataModel->GetChild("Workspace"));
	lua_setglobal(DefaultState, "workspace");

	lua_pushcfunction(
		DefaultState,
		[](lua_State* L)
		{
			const char* kname = luaL_checkstring(L, 1);

			lua_pushboolean(L, UserInput::IsKeyDown(SDL_KeyCode(kname[0])));
			
			return 1;
		},
		"input_keypressed"
	);
	lua_setglobal(DefaultState, "input_keypressed");

	lua_pushcfunction(
		DefaultState,
		// `lua_require` from `Luau/CLI/Repl.cpp` 18/09/2024
		[](lua_State* L)
		{
			std::string name = luaL_checkstring(L, 1);

			bool found = true;
			std::string sourceCode = FileRW::ReadFile(name, &found);

			if (!found)
				luaL_errorL(L, "error requiring module");

			// module needs to run in a new thread, isolated from the rest
			// note: we create ML on main thread so that it doesn't inherit environment of L
			lua_State* GL = lua_mainthread(L);
			lua_State* ML = lua_newthread(GL);
			lua_xmove(GL, L, 1);

			// new thread needs to have the globals sandboxed
			luaL_sandboxthread(ML);

			size_t bytecodeLength = 0;

			// now we can compile & run module on the new thread
			char* bytecode = luau_compile(sourceCode.c_str(), sourceCode.length(), nullptr, &bytecodeLength);
			if (luau_load(ML, name.c_str(), bytecode, bytecodeLength, 0) == 0)
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

			// there's now a return value on top of ML; L stack: _MODULES ML
			lua_xmove(ML, L, 1);
			lua_pushvalue(L, -1);
			//lua_setfield(L, -4, name.c_str());

			// L stack: _MODULES ML result
			if (lua_isstring(L, -1))
				lua_error(L);

			return 1;
		},
		"require"
	);
	lua_setglobal(DefaultState, "require");
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
	if (!m_L || m_StaleSource)
		this->Reload();

	if (m_HasUpdate)
	{
		lua_getglobal(m_L, "Update");

		if (!lua_isfunction(m_L, -1))
			m_HasUpdate = false;
		else
		{
			lua_pushnumber(m_L, dt);
			int succ = lua_pcall(m_L, 1, 0, 0);

			if (succ != 0)
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

	pushGameObject(m_L, this);
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

		int resumeResult = lua_resume(m_L, nullptr, 0);

		if (resumeResult == 0)
		{
			lua_getglobal(m_L, "Update");

			if (lua_isfunction(m_L, -1))
				m_HasUpdate = true;
		}
		else
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
