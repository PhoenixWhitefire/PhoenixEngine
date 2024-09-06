#include<luau/Compiler/include/luacode.h>
#include<luau/VM/include/lualib.h>

#include"gameobject/Script.hpp"
#include"datatype/GameObject.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

#define LUA_THROW(err) lua_pushstring(L, err); lua_error(L)
#define LUA_ASSERT(res, err, onfail) if (!res) { LUA_THROW(err); onfail; }

static RegisterDerivedObject<Object_Script> RegisterClassAs("Script");

static bool s_DidInitReflection = false;
static lua_State* DefaultState = nullptr;

static const std::unordered_map<Reflection::ValueType, lua_Type> ReflectionTypeToLuaType =
{
	{ Reflection::ValueType::None, lua_Type::LUA_TNIL },
	{ Reflection::ValueType::Bool, lua_Type::LUA_TBOOLEAN },
	{ Reflection::ValueType::Double, lua_Type::LUA_TNUMBER },
	{ Reflection::ValueType::Integer, lua_Type::LUA_TNUMBER },
	{ Reflection::ValueType::String, lua_Type::LUA_TSTRING }
};

// Returns a `Reflection::GenericValue` of the topmost value of the Luau stack
static Reflection::GenericValue luaTypeToGeneric(lua_State * L, int luaT, bool* successful)
{
	Reflection::GenericValue gv;

	*successful = true;

	switch (luaT)
	{
	case (lua_Type::LUA_TNIL):
	{
		gv.Type = Reflection::ValueType::None;
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
		const char* tname = luaL_typename(L, luaT);

		if (strcmp(tname, "Vector3") == 0)
		{
			Vector3 vec = *(Vector3*)lua_touserdata(L, -2);
			gv = vec.ToGenericValue();
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
		*successful = false;
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

	printf("new vec3 %f, %f, %f\n", vec.X, vec.Y, vec.Z);

	luaL_getmetatable(L, "Vector3");
	lua_setmetatable(L, -2);
}

static void pushGameObject(lua_State* L, GameObject* obj)
{
	void* ptrToObj = lua_newuserdata(L, sizeof(&obj));

	ptrToObj = &obj;

	luaL_getmetatable(L, "GameObject");
	lua_setmetatable(L, -2);
}

static void pushGenericValue(lua_State* L, Reflection::GenericValue& gv)
{
	switch (gv.Type)
	{
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
	case (Reflection::ValueType::Vector3):
	{
		pushVector3(L, gv);
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
template <class T> static void pushFunction(lua_State* L, T* obj, const char* name)
{
	// upvalues
	lua_pushlightuserdata(L, obj);
	lua_pushstring(L, name);

	lua_pushcclosure(
		L,

		[](lua_State* L)
		{
			T* refl = (T*)lua_touserdata(L, lua_upvalueindex(1));
			const char* fname = lua_tostring(L, lua_upvalueindex(2));
			std::string fnamestr = fname;

			auto& func = refl->GetFunction(fnamestr);
			const std::vector<Reflection::ValueType>& paramTypes = func.Inputs;

			int numParams = static_cast<int32_t>(paramTypes.size());
			int numArgs = lua_gettop(L);

			if (numArgs != numParams)
			{
				LUA_THROW(std::vformat(
					"Function '{}' expected {} arguments, got {} instead",
					std::make_format_args(fname, numParams, numArgs)
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

			Reflection::GenericValue output;// = func.Func(input);

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
		/*LUA_ASSERT(lua_isuserdata(L, -2)
			&& strcmp(luaTypeToString(L, lua_type(L, -2)), "GameObject") == 0,
			"Expected GameObject",
			return 0
		);*/
		LUA_ASSERT(lua_isstring(L, -1), "Expected index of type string", return 0);

		//lua_pushstring(L, "__type");
		//lua_gettable(L, -3);
		//printf("api_gameobjindex: arg mtt __type: %s\n", lua_tostring(L, -1));

		GameObject* obj = *(GameObject**)lua_touserdata(L, -2);
		const char* key = lua_tostring(L, -1);

		std::string keystr = key;

		//lua_getglobal(L, "GameObject");
		//lua_pushstring(L, key);
		//lua_rawget(L, -2);

		//// Pass-through to GameObject.new
		//if (!lua_isnil(L, -1))
		//	return 1;

		if (obj->HasProperty(key))
		{
			Reflection::GenericValue value = obj->GetPropertyValue(key);
			pushGenericValue(L, value);
		}
		if (obj->HasFunction(key))
		{
			pushFunction<GameObject>(L, obj, key);
		}
		else
		{
			GameObject* child = obj->GetChild(keystr);

			if (child)
				pushGameObject(L, child);
			else
			{
				std::string fullname = obj->GetFullName();

				LUA_THROW(std::vformat(
					"'{}' was neither a Property nor Child of {}",
					std::make_format_args(keystr, fullname)).c_str()
				);

				return 0;
			}
		}

		return 1;
	};

static auto api_gameobjnewindex = [](lua_State* L)
	{
		/*LUA_ASSERT(lua_isuserdata(L, -3)
			&& strcmp(luaTypeToString(L, lua_type(L, -3)), "GameObject") == 0,
			"Expected GameObject",
			return 0
		);*/
		LUA_ASSERT(lua_isstring(L, -2), "Expected index of type string", return 0);

		GameObject* obj = (GameObject*)lua_touserdata(L, -3);
		const char* key = lua_tostring(L, -2);

		std::string keystr = key;

		for (auto& p : obj->GetProperties())
			printf("%s\n", p.first.c_str());

		if (obj->HasProperty(key))
		{
			auto& prop = obj->GetProperty(key);

			int iArgType = lua_type(L, -1);
			lua_Type argType = (lua_Type)iArgType;

			lua_Type desiredType = ReflectionTypeToLuaType.find(prop.Type)->second;

			if (desiredType != argType)
			{
				const char* argTName = luaL_tolstring(L, -1, NULL);
				const char* desiredTName = luaL_typename(L, (int)desiredType);
				LUA_THROW(std::vformat(
					"Expected type {} for member {}, got {} instead",
					std::make_format_args(desiredTName, key, argTName)
				).c_str());

				return 0;
			}
			else
			{
				bool wasSuccessful = true;
				Reflection::GenericValue newvalue = luaTypeToGeneric(L, desiredType, &wasSuccessful);

				if (wasSuccessful)
					obj->SetPropertyValue(key, newvalue);
			}
		}
		else
			LUA_THROW((std::vformat(
				"Attempt to set invalid Member '{}' of GameObject",
				std::make_format_args(key)
			)).c_str());

		return 0;
	};

static auto api_gameobjecttostring = [](lua_State* L)
	{
		GameObject* obj = *(GameObject**)lua_touserdata(L, -1);
		lua_pushstring(L, obj->GetFullName().c_str());
		
		return 1;
	};

static auto api_newvec3 = [](lua_State* L)
	{
		printf("nargs: %i\n", lua_tointeger(L, 0));

		int isx = true;
		int isy = true;
		int isz = true;

		double x = lua_tonumberx(L, -3, &isx) || 0.f;
		double y = lua_tonumberx(L, -2, &isy) && isx || 0.f;
		double z = lua_tonumberx(L, -1, &isz) && isy || 0.f;

		printf("nums: %f, %f, %f\nnummask: %i, %i, %i\n", x, y, z, isx, isy, isz);

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
		LUA_ASSERT(lua_isstring(L, -1), "Expected index of type string", return 0);

		Vector3* vec = (Vector3*)lua_touserdata(L, -2);
		const char* key = lua_tostring(L, -1);

		lua_getglobal(L, "Vector3");
		lua_pushstring(L, key);
		lua_rawget(L, -2);

		// Pass-through to Vector3.new
		if (!lua_isnil(L, -1))
			return 1;

		std::string keystr = key;

		if (vec->HasProperty(key))
		{
			Reflection::GenericValue value = vec->GetPropertyValue(key);
			pushGenericValue(L, value);

			return 1;
		}
		else if (vec->HasFunction(key))
		{
			pushFunction<Vector3>(L, vec, key);

			return 1;
		}
		else
		{
			LUA_THROW(std::vformat(
				"{} is not a valid member of Vector3",
				std::make_format_args(key)
			).c_str());
			return 0;
		}
	};

static auto api_vec3newindex = [](lua_State* L)
	{
		LUA_THROW("Vector3s are immutable");

		return 0;
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

		lua_pushcfunction(DefaultState, api_vec3newindex, "Vector3.__newindex");
		lua_setfield(DefaultState, -2, "__newindex");

		lua_pushcfunction(DefaultState, api_vec3tostring, "Vector3.__tostring");
		lua_setfield(DefaultState, -2, "__tostring");

		lua_pushstring(DefaultState, "Vector3");
		lua_setfield(DefaultState, -2, "__type");
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
			scr->SourceFile = newval.String;
			scr->Reload();
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
	this->Reload();
}

void Object_Script::Update(double dt)
{
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
	{
		m_L = lua_newthread(DefaultState);
	}

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
			int topidx = lua_gettop(m_L);
			const char* errstr = lua_tostring(m_L, topidx);

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
