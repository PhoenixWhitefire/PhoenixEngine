// why would they do this...
// need to include a .cpp for the lua_exception class
#include<../../../LuauLib/VM/src/ldo.cpp>
#include<lualib.h>

#include"gameobject/Script.hpp"
#include"datatype/GameObject.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

#define LUA_THROW(err) lua_pushstring(L, err); lua_error(L)

DerivedObjectRegister<Object_Script> Object_Script::RegisterClassAs("Script");

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
					"Luau error: {}: {}",
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

		auto api_newobject = [](lua_State* L)
			{
				Object_Script* script = (Object_Script*)lua_topointer(L, lua_upvalueindex(1));

				if (!lua_isstring(L, -2))
				{
					/*auto t = lua_TypeToString((lua_Type)lua_type(L, 1)).c_str();
					const std::string fmtstr = "newobject called with arg of type {}, but should be string!";

					LUA_THROW(std::vformat(fmtstr, t));*/

					throw("incorrect type for newobject");
				}

				std::shared_ptr<GameObject> newObject = GameObjectFactory::CreateGameObject(lua_tostring(L, -2));
				newObject->SetParent(GameObject::DataModel);

				if (lua_isstring(L, -1))
				{
					const char* name = lua_tostring(L, -1);
					newObject->Name = name;
				}

				script->newobjs.push_back(newObject);

				return 0;
			};

		auto api_setpropv3 = [](lua_State* L) -> int
			{
				Object_Script* script = (Object_Script*)lua_topointer(L, lua_upvalueindex(1));

				if (!lua_isstring(L, -5))
					throw("Blud isn't a string :skull:");

				std::string targetname = lua_tostring(L, -5);
				std::string propname = lua_tostring(L, -4);
				int isx = true;
				int isy = true;
				int isz = true;
				
				double x = lua_tonumberx(L, -3, &isx);
				double y = lua_tonumberx(L, -2, &isy);
				double z = lua_tonumberx(L, -1, &isz);

				auto target = GameObject::DataModel->GetChild(targetname);

				if (!target)
				{
					std::string errstr = std::vformat(
						"Target not found for setpropvec3 {}",
						std::make_format_args(targetname)
					);
					lua_pushstring(L, errstr.c_str());
					lua_error(L);
				}

				auto propinfo = target->GetProperty(propname);

				if (!propinfo)
				{
					std::string errstr = std::vformat(
						"Inv prop for {} named {}",
						std::make_format_args(targetname, propname)
					);
					lua_pushstring(L, errstr.c_str());
					lua_error(L);
				}

				if (propinfo->Type != PropType::Vector3)
				{
					int typeId = (int)propinfo->Type;

					std::string errstr = std::vformat(
						"Prop for {} named {} not vec3, instead type id {}",
						std::make_format_args(targetname, propname, typeId)
					);
					lua_pushstring(L, errstr.c_str());
					lua_error(L);
				}

				GenericType gt;
				gt.Type = PropType::Vector3;
				gt.Vector3 = Vector3(x, y, z);

				propinfo->Reflection.Setter(gt);

				return 0;
			};

		lua_pushlightuserdata(L, this);
		lua_pushcclosure(L, api_newobject, "newobject", 1);
		lua_setglobal(L, "newobject");

		lua_pushlightuserdata(L, this);
		lua_pushcclosure(L, api_setpropv3, "setpropvec3", 1);
		lua_setglobal(L, "setpropvec3");

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
					"Luau error: {}: {}",
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
