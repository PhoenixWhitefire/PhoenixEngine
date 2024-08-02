// why would they do this...
// need to include a .cpp for the lua_exception class
#include<../../../LuauLib/VM/src/ldo.cpp>
#include<lualib.h>

#include"gameobject/Script.hpp"
#include"datatype/GameObject.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

#define LUA_THROW(err) lua_pushstring(L, err); lua_error(L)
#define LUA_ASSERT(res, err) if (!res) { LUA_THROW(err); }

DerivedObjectRegister<Object_Script> Object_Script::RegisterClassAs("Script");

struct IGameObject
{
	GameObject* ptr;
};

static std::string lua_TypeToString(lua_Type t)
{
	switch (t)
	{
	case (lua_Type::LUA_TBOOLEAN):
		return "bool";
	case (lua_Type::LUA_TNIL):
		return "nil";
	case (lua_Type::LUA_TNUMBER):
		return "number";
	case (lua_Type::LUA_TSTRING):
		return "string";
	case (lua_Type::LUA_TVECTOR):
		return "vector";
	case (lua_Type::LUA_TFUNCTION):
		return "function";
	case (lua_Type::LUA_TTABLE):
		return "table";

	default:
	{
		int iType = int(t);
		return std::vformat("T:{}", std::make_format_args(iType));
	}
	}
}

static void pushGameObject(lua_State* L, GameObject* obj)
{
	void* ptrToObj = lua_newuserdata(L, sizeof(IGameObject));
	new (ptrToObj) IGameObject();

	((IGameObject*)ptrToObj)->ptr = obj;

	luaL_getmetatable(L, "GameObjectMetatable");
	lua_setmetatable(L, -2);
}

static auto api_newobject = [](lua_State* L)
	{
		Object_Script* script = (Object_Script*)lua_topointer(L, lua_upvalueindex(1));

		if (!lua_isstring(L, -1))
		{
			std::string t = lua_TypeToString((lua_Type)lua_type(L, 1));
			const std::string fmtstr = "newobject called with arg of type {}, but should be string!";

			LUA_THROW(std::vformat(fmtstr, std::make_format_args(t)).c_str());
		}

		std::shared_ptr<GameObject> newObject = GameObjectFactory::CreateGameObject(lua_tostring(L, -1));

		pushGameObject(L, newObject.get());

		return 1;
	};

static auto api_gameobjindex = [](lua_State* L)
	{
		LUA_ASSERT(lua_isuserdata(L, -2), "Expected userdata");
		LUA_ASSERT(lua_isstring(L, -1), "Expected index of type string");

		GameObject* obj = (GameObject*)lua_touserdata(L, -2);
		const char* key = lua_tostring(L, -1);

		if (strcmp(key, "Name") == 0)
			lua_pushstring(L, obj->Name.c_str());

		else if (strcmp(key, "ClassName") == 0)
			lua_pushstring(L, obj->Name.c_str());

		else if (strcmp(key, "Parent") == 0)
		{
			if (!obj->Parent)
				lua_pushnil(L);
			else
				pushGameObject(L, obj->Parent.get());
		}

		return 1;
	};

static auto api_gameobjnewindex = [](lua_State* L)
	{
		return 0;
	};

static auto api_vec3index = [](lua_State* L)
	{
		LUA_ASSERT(lua_isuserdata(L, -2), "Expected userdata");
		LUA_ASSERT(lua_isstring(L, -1), "Expected index of type string");

		Vector3* vec = (Vector3*)lua_touserdata(L, -2);
		const char* key = lua_tostring(L, -1);

		if (strcmp(key, "X") == 0)
			lua_pushnumber(L, vec->X);

		else if (strcmp(key, "Y") == 0)
			lua_pushnumber(L, vec->Y);

		else if (strcmp(key, "Z") == 0)
			lua_pushnumber(L, vec->Z);

		else if (strcmp(key, "Z") == 0)
			lua_pushnumber(L, vec->Z);

		else
		{
			lua_getglobal(L, "Vector3");
			lua_pushstring(L, key);
			lua_rawget(L, -2);
		}

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

Object_Script::Object_Script()
{
	this->Name = "Script";
	this->ClassName = "Script";
	this->m_bytecode = NULL;

	this->L = lua_newstate(l_alloc, nullptr);

	m_properties.insert(std::pair(
		"SourceFile",
		PropInfo
		{
			PropType::String,
			PropReflection
			{
				[this]() { return GenericType{ PropType::String, this->SourceFile }; },
				[this](GenericType gt) { this->SourceFile = gt.String; }
			}
		}
	));

	m_procedures.insert(std::pair(
		"Reload",
		[this]() { this->Reload(); }
	));
}

void Object_Script::Initialize()
{
	this->Reload();
}

void Object_Script::Update(double dt)
{
	if (this->hasUpdate)
	{
		lua_getglobal(L, "Update");

		if (!lua_isfunction(L, -1))
			this->hasUpdate = false;
		else
		{
			lua_pushnumber(L, dt);
			int succ = lua_pcall(L, 1, 0, 0);

			if (succ != 0)
			{
				this->hasUpdate = false;

				int topidx = lua_gettop(L);
				const char* errstr = lua_tostring(L, topidx);
				std::string fullname = this->GetFullName();

				Debug::Log(std::vformat(
					"Luau runtime error: {}: {}",
					std::make_format_args(fullname, errstr)
				));
			}
		}
	}
}

bool Object_Script::LoadScript(std::string scriptFile)
{
	if (SourceFile == scriptFile)
		return true;

	this->SourceFile = scriptFile;

	return this->Reload();
}

bool Object_Script::Reload()
{
	bool fileExists = true;
	m_source = FileRW::ReadFile(SourceFile, &fileExists);

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

	if (this->L)
		lua_close(L);

	this->L = lua_newstate(l_alloc, nullptr);
	luaL_openlibs(L); // Load Standard Library ('print' etc)

	std::string FullName = this->GetFullName();
	std::string chunkname = std::vformat("{}", std::make_format_args(FullName));

	// FROM: https://github.com/luau-lang/luau/ README

	// needs lua.h and luacode.h
	size_t bytecodeSize = 0;
	this->m_bytecode = luau_compile(m_source.c_str(), m_source.length(), NULL, &bytecodeSize);

	int result = luau_load(L, chunkname.c_str(), m_bytecode, bytecodeSize, 0);
	
	free(m_bytecode);

	if (result == 0)
	{
		// Run the script

		//pushGameObject(L, GameObject::DataModel.get());
		//lua_setglobal(L, "game");

		lua_newtable(L);
		int vec3MTIdx = lua_gettop(L);
		lua_pushvalue(L, vec3MTIdx);
		lua_setglobal(L, "Vector3");

		/*lua_pushcfunction(L, [](lua_State* L)
			{

				printf("KILL YOURSELF\n");
				int isx = true;
				int isy = true;
				int isz = true;

				double x = lua_tonumberx(L, -3, &isx);
				double y = lua_tonumberx(L, -2, &isy);
				double z = lua_tonumberx(L, -1, &isz);

				Vector3* newvec = (Vector3*)lua_newuserdata(L, sizeof(Vector3));
				*newvec = Vector3(isx ? x : 0.f, (isx && isy) ? y : 0.f, (isx && isy && isz) ? z : 0.f);

				luaL_getmetatable(L, "Vector3Metatable");
				lua_setmetatable(L, -2);

				return 1;
			}, "Vector3.new");*/

		try
		{
			lua_pushstring(L, "KILL YOURSELF");
			lua_setfield(L, -2, "new");
		}
		catch (lua_exception e)
		{
			throw(e.what());
		}

		luaL_newmetatable(L, "Vector3Metatable");

		lua_pushstring(L, "__index");
		lua_pushcfunction(L, api_vec3index, "Vector3.__index");
		lua_settable(L, -4);

		/*lua_newtable(L);
		int gameObjTableIdx = lua_gettop(L);
		lua_pushvalue(L, gameObjTableIdx);

		lua_pushcfunction(L, api_newobject, "GameObject.new");
		lua_setfield(L, -2, "new");

		lua_pushcfunction(L, [](lua_State* L)
			{
				GameObject* gameobj = (GameObject*)lua_touserdata(L, -2);

				auto gt = GenericType();
				gt.Vector3 = *(Vector3*)lua_touserdata(L, -1);

				gameobj->SetProperty("Position", gt);

				return 0;
			}, "GameObjectMove");
		lua_setfield(L, -2, "SetPosition");

		lua_setglobal(L, "GameObject");*/

		/*luaL_newmetatable(L, "GameObjectMetatable");

		lua_pushvalue(L, gameObjTableIdx);
		lua_setfield(L, -3, "__index");*/

		
		/*
		lua_pushvalue(L, gameObjTableIdx);
		lua_setfield(L, -3, "__newindex");*/
		
		try
		{
			int resumeresult = lua_resume(L, L, 0);

			if (resumeresult == 0)
			{
				lua_getglobal(L, "Update");

				if (lua_isfunction(L, -1))
					this->hasUpdate = true;
			}
			else
			{
				int topidx = lua_gettop(L);
				const char* errstr = lua_tostring(L, topidx);

				Debug::Log(std::vformat(
					"Luau script init error: {}: {}",
					std::make_format_args(FullName, errstr)
				));
			}
				
		}
		catch (lua_exception e)
		{
			const char* what = e.what();
			Debug::Log(std::vformat("Exception {}: {}", std::make_format_args(FullName, what)));
			return false;
		}
		return true; /* return chunk main function */
	}
	else
	{
		int topidx = lua_gettop(L);
		const char* errstr = lua_tostring(L, topidx);

		Debug::Log(std::vformat("Luau compile error {}: {}: '{}'", std::make_format_args(result, this->Name, errstr)));
		return false;
	}
}
