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

#include"gameobject/Script.hpp"
#include"datatype/GameObject.hpp"
#include"UserInput.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

#define LUA_THROW(err) throw(std::runtime_error(err))
#define LUA_ASSERT(res, err) if (!res) { LUA_THROW(err); }

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
};

// Returns a `Reflection::GenericValue` of the topmost value of the Luau stack
static Reflection::GenericValue luaTypeToGeneric(lua_State* L, int luaT, bool* successful)
{
	Reflection::GenericValue gv;

	*successful = true;

	switch (luaT)
	{
	case (lua_Type::LUA_TNIL):
	{
		gv.Type = Reflection::ValueType::Null;
		break;
	}
	case (lua_Type::LUA_TBOOLEAN):
	{
		gv.Bool = lua_toboolean(L, -1);
		gv.Type = Reflection::ValueType::Bool;
		break;
	}
	case (lua_Type::LUA_TNUMBER):
	{
		gv.Double = lua_tonumber(L, -1);
		gv.Type = Reflection::ValueType::Double;
		break;
	}
	case (lua_Type::LUA_TSTRING):
	{
		gv.String = lua_tostring(L, -1);
		gv.Type = Reflection::ValueType::String;
		break;
	}
	case (lua_Type::LUA_TUSERDATA):
	{
		// IMPORTANT!!
		// Requires `LuauPreserveLudataRenaming` to be enabled in `Luau/VM/src/ltm.cpp`
		// 11/09/2024
		const char* tname = luaL_typename(L, -1);
		
		if (strcmp(tname, "Vector3") == 0)
		{
			Vector3 vec = *(Vector3*)lua_touserdata(L, -1);
			gv = vec.ToGenericValue();
		}
		else if (strcmp(tname, "Matrix") == 0)
		{
			gv = Reflection::GenericValue(*(glm::mat4*)lua_touserdata(L, -1));
		}
		else if (strcmp(tname, "GameObject") == 0)
		{
			gv.Type = Reflection::ValueType::GameObject;
			gv.Integer = *(uint32_t*)lua_touserdata(L, -1);
		}

		break;
	}
	default:
	{
		const char* tname = luaL_typename(L, luaT);
		LUA_THROW(std::vformat(
			"Could not convert type {} to a GenericValue (no conversion case)",
			std::make_format_args(tname)).c_str()
		);
	}
	}

	return gv;
}

static void pushVector3(lua_State* L, Vector3 vec)
{
	void* ptrTovec = lua_newuserdata(L, sizeof(Vector3));
	new (ptrTovec) Vector3();

	Vector3* vec3PtrToVec = (Vector3*)ptrTovec;
	vec3PtrToVec->X = vec.X;
	vec3PtrToVec->Y = vec.Y;
	vec3PtrToVec->Z = vec.Z;
	vec3PtrToVec->Magnitude = vec.Magnitude;

	luaL_getmetatable(L, "Vector3");
	lua_setmetatable(L, -2);
}

static void pushGameObject(lua_State* L, GameObject* obj)
{
	uint32_t* ptrToObj = (uint32_t*)lua_newuserdata(L, sizeof(uint32_t));

	*ptrToObj = obj->ObjectId;

	luaL_getmetatable(L, "GameObject");
	lua_setmetatable(L, -2);
}

static void pushGenericValue(lua_State* L, Reflection::GenericValue& gv)
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
		lua_pushboolean(L, gv.Bool);
		break;
	}
	case (Reflection::ValueType::Integer):
	{
		lua_pushinteger(L, static_cast<int32_t>(gv.Integer));
		break;
	}
	case (Reflection::ValueType::Double):
	{
		lua_pushnumber(L, gv.Double);
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
		glm::mat4* ptr = (glm::mat4*)lua_newuserdata(L, sizeof(glm::mat4));
		*ptr = gv.AsMatrix();

		luaL_getmetatable(L, "Matrix");
		lua_setmetatable(L, -2);
		break;
	}
	case (Reflection::ValueType::GameObject):
	{
		pushGameObject(L, GameObject::GetObjectById(static_cast<uint32_t>(gv.Integer)));
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
		LUA_THROW(std::vformat(
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
			luaL_checkudata(L, 1, "GameObject");
			GameObject* refl = GameObject::GetObjectById(*(uint32_t*)lua_touserdata(L, -1));
			const char* fname = lua_tostring(L, lua_upvalueindex(1));
			std::string fnamestr = fname;

			auto& func = refl->GetFunction(fnamestr);
			const std::vector<Reflection::ValueType>& paramTypes = func.Inputs;

			int numParams = static_cast<int32_t>(paramTypes.size());
			// -1 because the object is passed in as the first argument
			// (also means we don't need to add the object as an upvalue)
			// 11/09/2024
			int numArgs = lua_gettop(L) - 1;

			if (numArgs != numParams)
			{
				std::string argsString;

				for (int arg = 1; arg <= numArgs; arg++)
					argsString += std::string(luaL_typename(L, 0 - (numArgs - arg))) + ", ";

				argsString = argsString.substr(0, argsString.size() - 3);

				LUA_THROW(std::vformat(
					"Function '{}' expected {} arguments, got {} instead: {}",
					std::make_format_args(fname, numParams, numArgs, argsString)
				).c_str());

				return 0;
			}

			std::vector<Reflection::GenericValue> inputsList;

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

				int expectedLuaType = (int)ReflectionTypeToLuaType.find(paramType)->second;
				int actualLuaType = lua_type(L, argStackIndex);

				printf("pt: %i, exp lua type: %i\n", (int)paramType, expectedLuaType);

				if (actualLuaType != expectedLuaType)
				{
					const char* expectedName = luaL_typename(L, expectedLuaType);
					const char* actualName = luaL_tolstring(L, argStackIndex, NULL);

					// I get that shitty fucking ::vformat can't handle
					// a SINGULAR fucking parameter that isn't an lvalue,
					// but an `int`?? A literal fucking scalar??? What is this bullshit????
					int indexAsLuaIndex = index + 1;

					LUA_THROW(std::vformat(
						"Argument {} expected to be of type {}, but was {} instead (stack index {})",
						std::make_format_args(
							indexAsLuaIndex,
							expectedName,
							actualName,
							argStackIndex
						)
					).c_str());

					return 0;
				}
				else
				{
					bool conversionSuccessful = true;
					inputsList.push_back(luaTypeToGeneric(L, expectedLuaType, &conversionSuccessful));
				}
			}

			// Now, onto the *REAL* business...
			Reflection::GenericValue input;
			input.Type = Reflection::ValueType::Array;
			input.Array = inputsList;

			Reflection::GenericValue output;

			try
			{
				output = func.Func(refl, input);
			}
			catch (std::string err)
			{
				LUA_THROW(err.c_str());
			}
			catch (const char* err)
			{
				LUA_THROW(err);
			}

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
		2
	);
}

static auto api_newobject = [](lua_State* L)
	{
		if (!lua_isstring(L, -1))
		{
			std::string t = luaL_tolstring(L, -1, NULL);
			const std::string fmtstr = "newobject called with arg of type {}, but should be string!";

			LUA_THROW(std::vformat(fmtstr, std::make_format_args(t)).c_str());
		}

		GameObject* newObject = GameObject::CreateGameObject(lua_tostring(L, -1));

		pushGameObject(L, newObject);

		return 1;
	};

static auto api_gameobjindex = [](lua_State* L)
	{
		luaL_checkudata(L, 1, "GameObject");
		luaL_checkstring(L, 2);

		uint32_t objId = *(uint32_t*)lua_touserdata(L, -2);
		GameObject* obj = GameObject::GetObjectById(objId);
		const char* key = lua_tostring(L, -1);

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

				LUA_THROW(std::vformat(
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
		luaL_checkudata(L, 1, "GameObject");
		luaL_checkstring(L, 2);

		uint32_t objId = *(uint32_t*)lua_touserdata(L, -3);
		GameObject* obj = GameObject::GetObjectById(objId);
		const char* key = lua_tostring(L, -2);

		// Refer the `!obj` clause in `api_gameobjindex`
		if (!obj)
		{
			// Uh
			// Idk
			// Just give an `attempt to index nil`
			lua_pushnil(L);
			lua_setfield(L, -1, key);
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
			{
				throw(std::runtime_error(std::vformat(
					"Cannot set Property {}::{} to {} because it is read-only",
					std::make_format_args(obj->ClassName, key, argAsString)
				).c_str()));
			}

			auto reflToLuaIt = ReflectionTypeToLuaType.find(prop.Type);

			if (reflToLuaIt == ReflectionTypeToLuaType.end())
			{
				const std::string& typeName = Reflection::TypeAsString(prop.Type);
				throw(std::vformat(
					"No defined mapping between Reflection::ValueType::{} and a Lua type",
					std::make_format_args(typeName)
				));
			}

			lua_Type desiredType = reflToLuaIt->second;

			if (desiredType != argType && (desiredType != LUA_TUSERDATA && argType != LUA_TNIL))
			{
				const char* desiredTypeName = lua_typename(L, (int)desiredType);
				LUA_THROW(std::vformat(
					"Expected type {} for member {}::{}, got {} instead",
					std::make_format_args(desiredTypeName, obj->ClassName, key, argTypeName)
				).c_str());
			}
			else
			{
				lua_pushvalue(L, 3);

				bool wasSuccessful = true;
				Reflection::GenericValue newValue = luaTypeToGeneric(L, desiredType, &wasSuccessful);

				if (wasSuccessful)
					obj->SetPropertyValue(key, newValue);
			}
		}
		else
		{
			LUA_THROW((std::vformat(
				"Attempt to set invalid Member '{}' of GameObject",
				std::make_format_args(key)
			)).c_str());
		}

		return 0;
	};

static auto api_gameobjecttostring = [](lua_State* L)
	{
		uint32_t objId = *(uint32_t*)lua_touserdata(L, -1);
		GameObject* object = GameObject::GetObjectById(objId);
		
		if (object)
			lua_pushstring(L, object->GetFullName().c_str());
		else
			lua_pushnil(L);

		return 1;
	};

static auto api_newvec3 = [](lua_State* L)
	{
		double x = lua_tonumber(L, -3);
		double y = lua_tonumber(L, -2);
		double z = lua_tonumber(L, -1);

		pushVector3(L, Vector3(x, y, z));

		return 1;
	};

static auto api_vec3index = [](lua_State* L)
	{
		/*LUA_ASSERT(lua_isuserdata(L, -2)
			&& strcmp(luaTypeToString(L, lua_type(L, -2)), "Vector3") == 0,
			"Expected Vector3",
			return 0
		);*/
		LUA_ASSERT(lua_isstring(L, -1), "Expected index of type string");

		Vector3* vec = (Vector3*)lua_touserdata(L, -2);
		const char* key = lua_tostring(L, -1);

		lua_getglobal(L, "Vector3");
		lua_pushstring(L, key);
		lua_rawget(L, -2);

		// Pass-through to Vector3.new
		if (!lua_isnil(L, -1))
			return 1;

		if (vec->HasProperty(key))
		{
			Reflection::GenericValue value = vec->GetPropertyValue(key);
			pushGenericValue(L, value);

			return 1;
		}
		else if (vec->HasFunction(key))
		{
			//pushFunction<Vector3>(L, vec, key);

			//return 1;

			return 0;
		}
		else
		{
			LUA_THROW(std::vformat(
				"{} is not a valid member of Vector3",
				std::make_format_args(key)
			).c_str());
		}
	};

static auto api_vec3newindex = [](lua_State*)
	{
		LUA_THROW("Vector3s are immutable");
	};

static auto api_vec3tostring = [](lua_State* L)
	{
		Vector3 vec = *(Vector3*)lua_touserdata(L, -1);
		lua_pushstring(L, vec.ToString().c_str());

		return 1;
	};

static void* l_alloc(void* ud, void* ptr, size_t osize,
	size_t nsize) {
	(void)ud;  (void)osize;  /* not used */
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
		luaL_newmetatable(DefaultState, "Matrix");

		lua_pushstring(DefaultState, "Matrix");
		lua_setfield(DefaultState, -2, "__type");

		lua_pushcfunction(
			DefaultState,
			[](lua_State* L)
			{
				glm::mat4& mtx = *(glm::mat4*)lua_touserdata(L, -3);
				int r = lua_tointeger(L, -2);
				int c = lua_tointeger(L, -1);

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
				glm::mat4& mtx = *(glm::mat4*)lua_touserdata(L, -4);
				int r = lua_tointeger(L, -3);
				int c = lua_tointeger(L, -2);
				float v = static_cast<float>(lua_tonumber(L, -1));

				mtx[r][c] = v;

				return 0;
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
			const char* kname = lua_tostring(L, -1);

			lua_pushboolean(L, UserInput::IsKeyDown(SDL_KeyCode(kname[0])));
			
			return 1;
		},
		"input_keypressed"
	);
	lua_setglobal(DefaultState, "input_keypressed");
}

void Object_Script::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_DECLAREPROP(
		"SourceFile",
		String,
		[](GameObject* refl)
		{
			return Reflection::GenericValue(dynamic_cast<Object_Script*>(refl)->SourceFile);
		},
		[](GameObject* refl, Reflection::GenericValue newval)
		{
			Object_Script* scr = dynamic_cast<Object_Script*>(refl);

			if (scr->SourceFile != newval.AsString())
			{
				scr->SourceFile = newval.AsString();
				scr->Reload();
			}
		}
	);

	//REFLECTION_DECLAREPROP_SIMPLE(Object_Script, SourceFile, String);
	REFLECTION_DECLAREFUNC(
		"Reload",
		{},
		{ Reflection::ValueType::Bool },
		[](GameObject* scr, Reflection::GenericValue)
		{
			bool reloadSuccess = dynamic_cast<Object_Script*>(scr)->Reload();
			return Reflection::GenericValue(reloadSuccess);
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
	if (!m_L)
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

				int topidx = lua_gettop(m_L);
				const char* errstr = lua_tostring(m_L, topidx);
				std::string fullname = this->GetFullName();

				Debug::Log(std::vformat(
					"Luau runtime error: {}: {}",
					std::make_format_args(fullname, errstr)
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

	return this->Reload();
}

bool Object_Script::Reload()
{
	// TODO 14/09/2024
	// Don't want the script to run and the `script` global to
	// report it is parented to `nil`. Only want it to run when
	// parented under the `DataModel`. A `::IsDescendantOf` would be
	// ideal, but it seems unnecessary for now
	if (!this->GetParent())
		return false;

	bool fileExists = true;
	m_Source = FileRW::ReadFile(SourceFile, &fileExists);

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
				"Luau script init error: {}: {}",
				std::make_format_args(FullName, errstr)
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
