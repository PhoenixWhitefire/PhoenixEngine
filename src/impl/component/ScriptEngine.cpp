#include <luau/VM/include/lualib.h>
#include <luau/VM/src/lstate.h>
#include <Luau/Compiler.h>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_mouse.h>

#include <tracy/Tracy.hpp>

#include "component/ScriptEngine.hpp"
#include "component/Transform.hpp"
#include "component/Mesh.hpp"

#include "datatype/GameObject.hpp"
#include "datatype/Color.hpp"
#include "asset/TextureManager.hpp"
#include "asset/MeshProvider.hpp"
#include "asset/ModelImporter.hpp"
#include "asset/SceneFormat.hpp"
#include "GlobalJsonConfig.hpp"
#include "IntersectionLib.hpp"
#include "ThreadManager.hpp"
#include "UserInput.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

bool ScriptEngine::s_BackendScriptWantGrabMouse = false;

std::vector<ScriptEngine::YieldedCoroutine> ScriptEngine::s_YieldedCoroutines{};
const std::unordered_map<Reflection::ValueType, lua_Type> ScriptEngine::ReflectedTypeLuaEquivalent =
{
		{ Reflection::ValueType::Null,        lua_Type::LUA_TNIL       },

		{ Reflection::ValueType::Boolean,     lua_Type::LUA_TBOOLEAN   },
		{ Reflection::ValueType::Integer,     lua_Type::LUA_TNUMBER    },
		{ Reflection::ValueType::Double,      lua_Type::LUA_TNUMBER    },
		{ Reflection::ValueType::String,      lua_Type::LUA_TSTRING    },
		{ Reflection::ValueType::Vector2,     lua_Type::LUA_TTABLE     },
		{ Reflection::ValueType::Vector3,     lua_Type::LUA_TVECTOR    },

		{ Reflection::ValueType::Color,       lua_Type::LUA_TUSERDATA  },
		{ Reflection::ValueType::Matrix,      lua_Type::LUA_TUSERDATA  },

		{ Reflection::ValueType::GameObject,  lua_Type::LUA_TUSERDATA  },

		{ Reflection::ValueType::Array,       lua_Type::LUA_TTABLE     },
		{ Reflection::ValueType::Map,         lua_Type::LUA_TTABLE     }
};

static void pushVector3(lua_State* L, const glm::vec3& vec)
{
	lua_pushvector(L, vec.x, vec.y, vec.z);
}

static void pushColor(lua_State* L, const Color& col)
{
	void* ptr = lua_newuserdata(L, sizeof(Color));
	*(Color*)ptr = col;

	luaL_getmetatable(L, "Color");
	lua_setmetatable(L, -2);
}

static void pushMatrix(lua_State* L, const glm::mat4& Matrix)
{
	void* ptrToMtx = lua_newuserdata(L, sizeof(Matrix));
	*(glm::mat4*)ptrToMtx = Matrix;

	luaL_getmetatable(L, "Matrix");
	lua_setmetatable(L, -2);
}

static void pushJson(lua_State* L, const nlohmann::json& v)
{
	switch (v.type())
	{
	case nlohmann::json::value_t::null:
	{
		lua_pushnil(L);
		break;
	}

	case nlohmann::json::value_t::boolean:
	{
		lua_pushboolean(L, (bool)v);
		break;
	}
	case nlohmann::json::value_t::number_integer:
	{
		lua_pushinteger(L, (int)v);
		break;
	}
	case nlohmann::json::value_t::number_unsigned:
	{
		lua_pushinteger(L, static_cast<int>((uint32_t)v));
		break;
	}
	case nlohmann::json::value_t::number_float:
	{
		lua_pushnumber(L, (float)v);
		break;
	}
	case nlohmann::json::value_t::string:
	{
		lua_pushstring(L, ((std::string)v).c_str());
		break;
	}
	case nlohmann::json::value_t::array:
	{
		lua_newtable(L);

		for (int i = 0; static_cast<size_t>(i) < v.size(); i++)
		{
			lua_pushinteger(L, i + 1);
			pushJson(L, v[i]);
			lua_settable(L, -3);
		}
		
		break;
	}
	case nlohmann::json::value_t::object:
	{
		lua_newtable(L);

		for (auto it = v.begin(); it != v.end(); ++it)
		{
			pushJson(L, it.value());
			lua_setfield(L, -2, it.key().c_str());
		}

		break;
	}
	default:
	{
		lua_pushstring(L, (std::string("< JSON Value : ") + v.type_name() + " >").c_str());
	}
	}
}

static void luaTableToJson(lua_State* L, nlohmann::json& json)
{
	lua_pushnil(L);
	while (lua_next(L, -2) != 0)
	{
		nlohmann::json v{};

		switch (lua_type(L, -1))
		{
		case LUA_TNIL:
		{
			lua_pop(L, 1);
			continue;
		}
		case LUA_TBOOLEAN:
		{
			v = (bool)lua_toboolean(L, -1);
			break;
		}
		case LUA_TNUMBER:
		{
			v = lua_tonumber(L, -1);
			break;
		}
		case LUA_TSTRING:
		{
			v = lua_tostring(L, -1);
			break;
		}
		case LUA_TTABLE:
		{
			luaTableToJson(L, v);
			break;
		}

		[[unlikely]] default:
		{
			const char* vtname = luaL_typename(L, -1);
			std::string k;

			if (lua_type(L, -2) == LUA_TNUMBER)
				k = std::to_string(lua_tonumber(L, -2));
			else
				k = lua_tostring(L, -2);

			RAISE_RT(std::format(
				"Key '{}' is of non-JSON type {}!",
				k, vtname
			));
		}
		}

		if (lua_type(L, -2) == LUA_TNUMBER)
			json[static_cast<size_t>(lua_tonumber(L, -2)) - 1] = v;
		else
			json[lua_tostring(L, -2)] = v;

		lua_pop(L, 1);
	}

}

int ScriptEngine::CompileAndLoad(lua_State* L, const std::string& SourceCode, const std::string& ChunkName)
{
	ZoneScoped;
	
	// Tell Luau that these are mutable. Otherwise, GETIMPORT optimizations
	// will cause them to be treated as constants and only invoke their `__index` functions
	// once and cache the result
	const char* mutableGlobals[] = 
	{
		"game", "workspace", "script",
		NULL
	};

	Luau::CompileOptions compileOptions;
	compileOptions.optimizationLevel = 2;
	compileOptions.debugLevel = 1;
	compileOptions.mutableGlobals = mutableGlobals;

	std::string bytecode = Luau::compile(SourceCode, compileOptions);

	return luau_load(L, ChunkName.c_str(), bytecode.data(), bytecode.size(), 0);
}

Reflection::GenericValue ScriptEngine::L::LuaValueToGeneric(lua_State* L, int StackIndex)
{
	switch (lua_type(L, StackIndex))
	{
	case LUA_TNIL:
	{
		return Reflection::GenericValue();
	}
	case LUA_TBOOLEAN:
	{
		return (bool)lua_toboolean(L, StackIndex);
	}
	case LUA_TNUMBER:
	{
		return lua_tonumber(L, StackIndex);
	}
	case LUA_TSTRING:
	{
		return lua_tostring(L, StackIndex);
	}
	case LUA_TVECTOR:
	{
		return glm::make_vec3(luaL_checkvector(L, StackIndex));
	}
	case LUA_TUSERDATA:
	{
		// IMPORTANT!!
		// Requires `LuauPreserveLudataRenaming` to be enabled in `Luau/VM/src/ltm.cpp`
		// 11/09/2024
		const char* tname = luaL_typename(L, StackIndex);

		if (strcmp(tname, "Color") == 0)
		{
			Color col = *(Color*)lua_touserdata(L, StackIndex);
			return col.ToGenericValue();
		}
		else if (strcmp(tname, "Matrix") == 0)
		{
			return *(glm::mat4*)lua_touserdata(L, StackIndex);
		}
		else if (strcmp(tname, "GameObject") == 0)
		{
			Reflection::GenericValue gv = *(uint32_t*)lua_touserdata(L, StackIndex);
			gv.Type = Reflection::ValueType::GameObject;

			return gv;
		}
		else
			luaL_error(L, "Couldn't convert a '%s' userdata to a GenericValue (unrecognized)", tname);
	}
	case LUA_TTABLE:
	{
		// 15/09/2024
		// TODO
		// Maps

		std::vector<Reflection::GenericValue> items;

		// https://www.lua.org/manual/5.1/manual.html#lua_next
		lua_pushnil(L);

		while (lua_next(L, -2) != 0)
		{
			items.push_back(ScriptEngine::L::LuaValueToGeneric(L, -1));
			lua_pop(L, 1);
		}

		return items;
	}
	default:
	{
		const char* tname = luaL_typename(L, StackIndex);
		luaL_error(L, "Could not convert type '%s' to a GenericValue (no conversion case)", tname);
	}
	}
}

void ScriptEngine::L::PushGenericValue(lua_State* L, const Reflection::GenericValue& gv)
{
	luaL_checkstack(L, 1, "::PushGenericValue");

	switch (gv.Type)
	{
	case Reflection::ValueType::Null:
	{
		lua_pushnil(L);
		break;
	}
	case Reflection::ValueType::Boolean:
	{
		lua_pushboolean(L, gv.AsBoolean());
		break;
	}
	case Reflection::ValueType::Integer:
	{
		lua_pushinteger(L, static_cast<int32_t>(gv.AsInteger()));
		break;
	}
	case Reflection::ValueType::Double:
	{
		lua_pushnumber(L, gv.AsDouble());
		break;
	}
	case Reflection::ValueType::String:
	{
		lua_pushlstring(L, gv.AsStringView().data(), gv.AsStringView().size());
		break;
	}
	case Reflection::ValueType::Vector3:
	{
		pushVector3(L, gv.AsVector3());
		break;
	}
	case Reflection::ValueType::Color:
	{
		pushColor(L, gv);
		break;
	}
	case Reflection::ValueType::Matrix:
	{
		pushMatrix(L, gv.AsMatrix());
		break;
	}
	case Reflection::ValueType::GameObject:
	{
		PushGameObject(L, GameObject::FromGenericValue(gv));
		break;
	}
	case Reflection::ValueType::Array:
	{
		std::span<Reflection::GenericValue> array = gv.AsArray();
		luaL_checkstack(L, 6, "::PushGenericValue of type Array");
		lua_newtable(L);

		for (int index = 0; static_cast<size_t>(index) < array.size(); index++)
		{
			lua_pushinteger(L, index + 1);
			L::PushGenericValue(L, array[index]);
			lua_settable(L, -3);
		}

		break;
	}
	case Reflection::ValueType::Map:
	{
		std::span<Reflection::GenericValue> array = gv.AsArray();

		if (array.size() % 2 != 0)
			RAISE_RT("GenericValue type was Map, but it does not have an even number of elements!");

		lua_newtable(L);

		for (int index = 0; static_cast<size_t>(index) < array.size(); index++)
		{
			L::PushGenericValue(L, array[index]);

			if ((index + 1) % 2 == 0)
				lua_settable(L, -3);
		}

		break;
	}
	default:
	{
		const std::string_view& typeName = Reflection::TypeAsString(gv.Type);
		luaL_error(L, "Cannot reflect values of type '%s'", typeName.data());
	}
	}
}

void ScriptEngine::L::PushGameObject(lua_State* L, GameObject* obj)
{
	if (!obj)
		lua_pushnil(L); // null object properties are nil and falsey
	else
	{
		uint32_t* ptrToObj = (uint32_t*)lua_newuserdatadtor(
			L,
			sizeof(uint32_t),
			[](void* ptrId)
			{
				uint32_t id = *(uint32_t*)ptrId;

				if (GameObject* o = GameObject::GetObjectById(id))
					o->DecrementHardRefs(); // removes the reference in `::Create`
			}
		);
		*ptrToObj = obj ? obj->ObjectId : PHX_GAMEOBJECT_NULL_ID;
		
		luaL_getmetatable(L, "GameObject");
		lua_setmetatable(L, -2);
	}
}

int ScriptEngine::L::HandleMethodCall(
	lua_State* L,
	Reflection::Method* func,
	std::pair<EntityComponent, uint32_t> FromComponent
)
{
	int numArgs = lua_gettop(L) - 1;

	const std::vector<Reflection::ParameterType>& paramTypes = func->Inputs;

	int numParams = static_cast<int32_t>(paramTypes.size());
	int minArgs = 0;

	for (int i = 0; i < numParams; i++)
	{
		const Reflection::ParameterType& param = paramTypes[i];

		if (!param.IsOptional)
			minArgs++;
		else
			break;
	}

	if (numArgs < minArgs)
	{
		std::string argsString = ": ( ";

		if (numArgs > 0)
		{
			for (int arg = 1; arg < numArgs + 1; arg++)
				argsString += std::string(luaL_typename(L, -(numArgs + 1 - arg))) + ", ";
			
			// trailing `, `
			argsString = argsString.substr(0, argsString.size() - 2);

			argsString += " )";
		}
		else
			argsString.clear();

		luaL_error(L,
			"Function expects at least %i arguments, got %i instead%s", 
			numParams, numArgs, argsString.c_str()
		);

		return 0;
	}
	else if (numArgs > numParams)
	{
		Log::Warning(std::format("Function received {} more arguments than necessary",
			numArgs - numParams
		));
	}

	std::vector<Reflection::GenericValue> inputs;

	// This *entire* for-loop is just for handling input arguments
	for (int index = 0; index < numArgs; index++)
	{
		Reflection::ValueType paramType = paramTypes[index];

		// Ex: W/ 3 args:
		// 0 = -3
		// 1 = -2
		// 2 = -1
		// Simpler than I thought actually
		int argStackIndex = index - numArgs;

		auto expectedLuaTypeIt = ScriptEngine::ReflectedTypeLuaEquivalent.find(paramType);

		if (expectedLuaTypeIt == ScriptEngine::ReflectedTypeLuaEquivalent.end())
			RAISE_RT(std::format(
				"Cannot verify that argument %i should be a %s",
				index + 1, Reflection::TypeAsString(paramType).data()
			));

		int expectedLuaType = (int)expectedLuaTypeIt->second;
		int actualLuaType = lua_type(L, argStackIndex);

		if (actualLuaType != expectedLuaType)
		{
			const char* expectedName = lua_typename(L, expectedLuaType);
			const char* actualTypeName = luaL_typename(L, argStackIndex);
			const char* providedValue = luaL_tolstring(L, argStackIndex, NULL);

			luaL_error(L,
				"Argument %i expected to be of type %s, but was '%s' (%s) instead",
				index + 1,
				expectedName,
				providedValue,
				actualTypeName
			);

			return 0;
		}
		else
			inputs.push_back(L::LuaValueToGeneric(L, argStackIndex));
	}

	// Now, onto the *REAL* business...
	std::vector<Reflection::GenericValue> outputs;

	try
	{
		outputs = func->Func(GameObject::ReflectorHandleToPointer(FromComponent), inputs);
	}
	catch (const std::runtime_error& err)
	{
		luaL_error(L, "%s", err.what());
	}

	assert(outputs.size() == func->Outputs.size());

	for (size_t i = 0; i < outputs.size(); i++)
	{
		const Reflection::GenericValue& output = outputs[i];

		assert(output.Type == func->Outputs[i]);
		L::PushGenericValue(L, output);
	}

	return (int)func->Outputs.size();

	// ... kinda expected more, but ngl i feel SOOOO gigabrain for
	// giving ::GenericValue an Array, like, it all just clicks in now!
	// And then Maps just being Arrays, except odd elements are the keys
	// and even elements are the values?! Call me Einstein already on god-
	// (Me writing this as Rendering is completely busted and I have no clue
	// why oh no
	// 15/08/2024
}

void ScriptEngine::L::PushMethod(lua_State* L, Reflection::Method* Function, std::pair<EntityComponent, uint32_t> FromComponent)
{
	// if we dont do this then comparison will not work
	// ex: `game.Close == game.Close`

	lua_pushlightuserdata(L, Function);
	lua_rawget(L, LUA_ENVIRONINDEX);

	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1); // remove `nil`, stack empty

		static_assert(sizeof(FromComponent) <= sizeof(void*));
		void* data = nullptr;
		memcpy(&data, &FromComponent, sizeof(FromComponent));

		lua_pushlightuserdata(L, Function);
		lua_pushlightuserdata(L, data);

		lua_pushcclosure(
			L,
			[](lua_State* L)
			{
				Reflection::Method* fn = static_cast<Reflection::Method*>(lua_tolightuserdata(L, lua_upvalueindex(1)));
				auto& fc = *static_cast<decltype(FromComponent)*>(lua_tolightuserdata(L, lua_upvalueindex(2)));

				return ScriptEngine::L::HandleMethodCall(
					L,
					fn,
					fc
				);
			},

			"PushMethodThunk",
			1
		); // stack is now just closure

		lua_pushlightuserdata(L, Function); // stack: closure, lud
		lua_pushvalue(L, -2);               // stack: closure, lud, closure
		lua_settable(L, LUA_ENVIRONINDEX);  // map closure (value) to lud (key)

		// stack is now just closure
	}
	// value we fetch from `_ENVIRON` will be closure that was pushed earlier
}

static bool IsFdInProgress = false;
static std::vector<std::string> FdResults;

static void fdCallback(void*, const char* const* FileList, int)
{
	if (!FileList)
	{
		const char* err = SDL_GetError();
		RAISE_RT(err);
	}

	FdResults.clear();

	size_t nextIdx = 0;
	while (FileList[nextIdx])
	{
		FdResults.push_back(FileList[nextIdx]);

		// windows SMELLS :( 18/02/2025
		std::string& str = FdResults.back();
		size_t off = str.find_first_of("\\");

		while (off != std::string::npos)
			off = str.replace(off, 1, "/").find_first_of("\\");

		nextIdx++;
	}

	IsFdInProgress = false;
}

using namespace ScriptEngine;
using namespace ScriptEngine::L;

static std::pair<std::string_view, GlobalFn> s_GlobalFunctions[] =
{
	{
	"matrix_getv",
	{
		[](lua_State* L)
		{
			glm::mat4& mtx = *(glm::mat4*)luaL_checkudata(L, 1, "Matrix");
			int r = luaL_checkinteger(L, 2);
			int c = luaL_checkinteger(L, 3);

			lua_pushnumber(L, mtx[r][c]);

			return 1;
		},
		3
	}
	},

	{
	"matrix_setv",
	{
		[](lua_State* L)
		{
			glm::mat4& mtx = *(glm::mat4*)luaL_checkudata(L, 1, "Matrix");
			int r = luaL_checkinteger(L, 2);
			int c = luaL_checkinteger(L, 3);
			float v = static_cast<float>(luaL_checknumber(L, 4));

			mtx[r][c] = v;

			Reflection::GenericValue gv{ mtx };

			L::PushGenericValue(L, gv);

			return 1;
		},
		4
	}
	},

	{
	"input_keypressed",
	{
		[](lua_State* L)
		{
			if (ImGui::GetIO().WantCaptureKeyboard)
				lua_pushboolean(L, false);
			else
			{
				const char* kname = luaL_checkstring(L, 1);
				lua_pushboolean(L, UserInput::IsKeyDown(SDL_Keycode(kname[0])));
			}

			return 1;
		},
		0
	}
	},

	{
	"input_mouse_bdown",
	{
		[](lua_State* L)
		{
			if (ImGui::GetIO().WantCaptureMouse)
				lua_pushboolean(L, false);
			else
			{
				bool lmb = luaL_checkboolean(L, 1);
				lua_pushboolean(L, UserInput::IsMouseButtonDown(lmb));
			}

			return 1;
		},
		1
	}
	},

	{
	"input_mouse_getpos",
	{
		[](lua_State* L)
		{
			float mx = 0;
			float my = 0;

			SDL_GetMouseState(&mx, &my);

			lua_pushnumber(L, mx);
			lua_pushnumber(L, my);

			return 2;
		},
		0
	}
	},

	{
	"input_mouse_setlocked",
	{
		[](lua_State* L)
		{
			s_BackendScriptWantGrabMouse = luaL_checkboolean(L, 1);
			return 0;
		},
		0
	}
	},

	{
	"sleep",
	{
		[](lua_State* L)
		{
			double sleepTime = luaL_checknumber(L, 1);

			// TODO a kind of hack to get what script we're running as?
			lua_getglobal(L, "script");
			Reflection::GenericValue script = ScriptEngine::L::LuaValueToGeneric(L, -1);
			GameObject* scriptObject = GameObject::FromGenericValue(script);
			// modules currently do not have a `script` global
			uint32_t scriptId = scriptObject ? scriptObject->ObjectId : PHX_GAMEOBJECT_NULL_ID;

			lua_pushthread(L);

			auto& b = ScriptEngine::s_YieldedCoroutines.emplace_back(
				L,
				// make sure the coroutine doesn't get de-alloc'd before we resume it
				lua_ref(L, -1),
				scriptId,
				YieldedCoroutine::ResumptionMode::ScheduledTime
			);

			double curTime = GetRunningTime();

			b.RmSchedule = { curTime, curTime + sleepTime };

			return lua_yield(L, 1);
		},
		1
	}
	},

	{
	"require",
	// `lua_require` from `Luau/CLI/Repl.cpp` 18/09/2024
	{
		[](lua_State* L)
		{
			std::string name = luaL_checkstring(L, 1);

			bool found = true;
			std::string sourceCode = FileRW::ReadFile(name, &found);

			if (!found)
				luaL_errorL(L, "module not found");

			// module needs to run in a new thread, isolated from the rest
			// note: we create ML on main thread so that it doesn't inherit environment of L
			lua_State* GL = lua_mainthread(L);
			lua_State* ML = lua_newthread(GL);
			lua_xmove(GL, L, 1);

			// new thread needs to have the globals sandboxed
			luaL_sandboxthread(ML);

			// now we can compile & run module on the new thread
			if (CompileAndLoad(ML, sourceCode, "@" + name) == 0)
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
		1
	}
	},

	{
	"mesh_get",
	{
		[](lua_State* L)
		{
			const char* meshPath = luaL_checkstring(L, 1);

			MeshProvider* meshProvider = MeshProvider::Get();

			uint32_t meshId = meshProvider->LoadFromPath(meshPath, false, true);
			Mesh& mesh = meshProvider->GetMeshResource(meshId);

			lua_newtable(L);

			lua_newtable(L);

			for (size_t vi = 0; vi < mesh.Vertices.size(); vi++)
			{
				const Vertex& v = mesh.Vertices.at(vi);

				lua_pushinteger(L, static_cast<int32_t>(vi + 1));
				lua_newtable(L);

				pushVector3(L, v.Position);
				lua_setfield(L, -2, "Position");

				pushVector3(L, v.Normal);
				lua_setfield(L, -2, "Normal");

				lua_newtable(L);
				lua_pushnumber(L, v.Paint.x);
				lua_setfield(L, -2, "R");

				lua_pushnumber(L, v.Paint.y);
				lua_setfield(L, -2, "G");

				lua_pushnumber(L, v.Paint.z);
				lua_setfield(L, -2, "B");

				lua_pushnumber(L, v.Paint.w);
				lua_setfield(L, -2, "A");

				lua_setfield(L, -2, "Paint");

				lua_newtable(L);

				lua_pushinteger(L, 1);
				lua_pushnumber(L, v.TextureUV.x);
				lua_settable(L, -3);

				lua_pushinteger(L, 2);
				lua_pushnumber(L, v.TextureUV.y);
				lua_settable(L, -3);

				lua_setfield(L, -2, "UV");

				lua_settable(L, -3);
			}

			lua_setfield(L, -2, "Vertices");

			lua_newtable(L);

			for (uint32_t indexIndex = 0; indexIndex < mesh.Indices.size(); indexIndex++)
			{
				lua_pushinteger(L, indexIndex + 1);
				lua_pushinteger(L, static_cast<int32_t>(mesh.Indices.at(indexIndex)));
				lua_settable(L, -3);
			}

			lua_setfield(L, -2, "Indices");

			return 1;
		},
		1
	}
	},

	{
	"mesh_set",
	{
		[](lua_State* L)
		{
			const char* meshName = luaL_checkstring(L, 1);

			Mesh mesh;
			mesh.MeshDataPreserved = true;

			lua_getfield(L, -1, "Vertices");

			lua_pushnil(L);
			while (lua_next(L, -2) != 0)
			{
				lua_getfield(L, -1, "Position");
				glm::vec3 position = ScriptEngine::L::LuaValueToGeneric(L).AsVector3();

				lua_getfield(L, -2, "Normal");
				glm::vec3 normal = ScriptEngine::L::LuaValueToGeneric(L).AsVector3();

				lua_getfield(L, -3, "Paint");
				Reflection::GenericValue paintVal = ScriptEngine::L::LuaValueToGeneric(L);
				std::span<Reflection::GenericValue> paintgv = paintVal.AsArray();
				glm::vec4 paint{ paintgv[0].AsDouble(), paintgv[1].AsDouble(), paintgv[2].AsDouble(), paintgv[3].AsDouble() };
				
				lua_getfield(L, -4, "UV");

				std::vector<double> uvlist;

				lua_pushnil(L);
				while (lua_next(L, -2) != 0)
				{
					uvlist.push_back(lua_tonumber(L, -1));
					lua_pop(L, 1);
				}

				glm::vec2 uv =
				{
					uvlist.at(0),
					uvlist.at(1)
				};

				mesh.Vertices.emplace_back(Vertex{ position, normal, paint, uv });

				lua_pop(L, 5);
			}

			lua_getfield(L, -2, "Indices");

			lua_pushnil(L);
			while (lua_next(L, -2) != 0)
			{
				mesh.Indices.push_back(lua_tointeger(L, -1));
				lua_pop(L, 1);
			}

			MeshProvider* meshProvider = MeshProvider::Get();
			meshProvider->Assign(mesh, meshName, true);

			return 0;
		},
		1
	}
	},

	{
	"mesh_save",
	{
		[](lua_State* L)
		{
			const char* internalName = luaL_checkstring(L, 1);
			const char* savePath = luaL_checkstring(L, 2);

			MeshProvider* meshProv = MeshProvider::Get();
			meshProv->Save(meshProv->LoadFromPath(internalName), savePath);

			return 0;
		},
		2
	}
	},

	{
	"world_raycast",
	{
		[](lua_State* L)
		{
			GameObject* workspace = GameObject::GetObjectById(GameObject::s_DataModel)->FindChildWhichIsA("Workspace");

			if (!workspace)
				luaL_error(L, "A Workspace was not found within the DataModel");

			glm::vec3 origin = LuaValueToGeneric(L, 1).AsVector3();
			glm::vec3 vector = LuaValueToGeneric(L, 2).AsVector3();

			Reflection::GenericValue ignoreListVal = LuaValueToGeneric(L, 3);
			std::span<Reflection::GenericValue> providedIgnoreList = ignoreListVal.AsArray();

			std::vector<GameObject*> ignoreList;
			for (const Reflection::GenericValue& gv : providedIgnoreList)
				ignoreList.push_back(GameObject::FromGenericValue(gv));

			IntersectionLib::Intersection result;
			GameObject* hitObject = nullptr;
			double closestHit = INFINITY;

			for (GameObject* p : workspace->GetDescendants())
			{
				if (std::find(ignoreList.begin(), ignoreList.end(), p) != ignoreList.end())
					continue;

				EcMesh* object = p->GetComponent<EcMesh>();
				EcTransform* ct = p->GetComponent<EcTransform>();

				if (object && ct)
				{
					glm::vec3 pos = ct->Transform[3];
					glm::vec3 size = ct->Size;

					IntersectionLib::Intersection hit = IntersectionLib::LineAabb(
						origin,
						vector,
						pos,
						size
					);

					if (hit.Occurred)
						if (hit.Depth < closestHit)
						{
							result = hit;
							closestHit = hit.Depth;
							hitObject = p;
						}
				}
			}

			if (hitObject)
			{
				lua_newtable(L);

				ScriptEngine::L::PushGameObject(L, hitObject);
				lua_setfield(L, -2, "Object");

				Reflection::GenericValue posg{ result.Position };

				ScriptEngine::L::PushGenericValue(L, posg);
				lua_setfield(L, -2, "Position");

				Reflection::GenericValue normalg{ result.Normal };

				ScriptEngine::L::PushGenericValue(L, normalg);
				lua_setfield(L, -2, "Normal");
			}
			else
				lua_pushnil(L);

			return 1;
		},
		3
	}
	},

	{
	"world_aabbcast",
	{
		[](lua_State* L)
		{
			GameObject* workspace = GameObject::GetObjectById(GameObject::s_DataModel)->FindChildWhichIsA("Workspace");

			if (!workspace)
				luaL_error(L, "A Workspace was not found within the DataModel");

			glm::vec3 apos = LuaValueToGeneric(L, 1).AsVector3();
			glm::vec3 asize = LuaValueToGeneric(L, 2).AsVector3();

			Reflection::GenericValue ignoreListVal = LuaValueToGeneric(L, 3);
			std::span<Reflection::GenericValue> providedIgnoreList = ignoreListVal.AsArray();

			std::vector<GameObject*> ignoreList;
			for (const Reflection::GenericValue& gv : providedIgnoreList)
				ignoreList.push_back(GameObject::FromGenericValue(gv));

			IntersectionLib::Intersection result;
			GameObject* hitObject = nullptr;
			double closestHit = INFINITY;

			for (GameObject* p : workspace->GetDescendants())
			{
				if (std::find(ignoreList.begin(), ignoreList.end(), p) != ignoreList.end())
					continue;

				EcMesh* object = p->GetComponent<EcMesh>();
				EcTransform* ct = p->GetComponent<EcTransform>();

				if (object && ct)
				{
					glm::vec3 bpos = ct->Transform[3];
					glm::vec3 bsize = ct->Size;

					IntersectionLib::Intersection hit = IntersectionLib::AabbAabb(
						apos,
						asize,
						bpos,
						bsize
					);

					if (hit.Occurred)
						if (hit.Depth < closestHit)
						{
							result = hit;
							closestHit = hit.Depth;
							hitObject = p;
						}
				}
			}

			if (hitObject)
			{
				lua_newtable(L);

				ScriptEngine::L::PushGameObject(L, hitObject);
				lua_setfield(L, -2, "Object");

				Reflection::GenericValue posg = result.Position;

				ScriptEngine::L::PushGenericValue(L, posg);
				lua_setfield(L, -2, "Position");

				Reflection::GenericValue normalg = result.Normal;

				ScriptEngine::L::PushGenericValue(L, normalg);
				lua_setfield(L, -2, "Normal");
			}
			else
				lua_pushnil(L);

			return 1;
		},
		3
	}
	},

	{
	"world_aabbquery",
	{
		[](lua_State* L)
		{
			GameObject* workspace = GameObject::GetObjectById(GameObject::s_DataModel)->FindChildWhichIsA("Workspace");

			if (!workspace)
				luaL_error(L, "A Workspace was not found within the DataModel");

			glm::vec3 apos = LuaValueToGeneric(L, 1).AsVector3();
			glm::vec3 asize = LuaValueToGeneric(L, 2).AsVector3();

			Reflection::GenericValue ignoreListVal = LuaValueToGeneric(L, 3);
			std::span<Reflection::GenericValue> providedIgnoreList = ignoreListVal.AsArray();

			std::vector<GameObject*> ignoreList;
			for (const Reflection::GenericValue& gv : providedIgnoreList)
				ignoreList.push_back(GameObject::FromGenericValue(gv));

			IntersectionLib::Intersection result;
			std::vector<GameObject*> hits;

			for (GameObject* p : workspace->GetDescendants())
			{
				if (std::find(ignoreList.begin(), ignoreList.end(), p) != ignoreList.end())
					continue;

				EcTransform* object = p->GetComponent<EcTransform>();

				if (object)
				{
					glm::vec3 bpos = object->Transform[3];
					glm::vec3 bsize = object->Size;

					IntersectionLib::Intersection hit = IntersectionLib::AabbAabb(
						apos,
						asize,
						bpos,
						bsize
					);

					if (hit.Occurred)
						hits.push_back(p);
				}
			}

			lua_newtable(L);

			for (int index = 0; static_cast<size_t>(index) < hits.size(); index++)
			{
				lua_pushinteger(L, index);
				ScriptEngine::L::PushGameObject(L, hits[index]);
				lua_settable(L, -3);
			}

			return 1;
		},
		3
	}
	},

	{
	"model_import",
	{
		[](lua_State* L)
		{
			const char* path = luaL_checkstring(L, 1);

			const std::vector<GameObject*>& loaded = ModelLoader(path, nullptr).LoadedObjs;
			
			ScriptEngine::L::PushGenericValue(L, loaded.at(0)->ToGenericValue());

			/*
			lua_newtable(L);

			for (int index = 0; index < loadedRoots.size(); index++)
			{
				GameObject* object = loadedRoots[index];
				Reflection::GenericValue gv = object->ToGenericValue();

				lua_pushinteger(L, index);
				ScriptEngine::L::PushGenericValue(L, gv);

				lua_settable(L, -3);
			}
			*/

			return 1;
		},
		1
	}
	},

	{
	"scene_save",
	{
		[](lua_State* L)
		{
			Reflection::GenericValue rootNodesGv = ScriptEngine::L::LuaValueToGeneric(L, 1);
			const char* path = luaL_checkstring(L, 2);

			std::span<Reflection::GenericValue> rootNodesArray = rootNodesGv.AsArray();
			std::vector<GameObject*> rootNodes;

			for (const Reflection::GenericValue& gv : rootNodesArray)
				rootNodes.push_back(GameObject::FromGenericValue(gv));

			std::string fileContents = SceneFormat::Serialize(rootNodes, path);
			FileRW::WriteFileCreateDirectories(path, fileContents, false);

			lua_pushboolean(L, 1);
			lua_pushstring(L, "No errors occurred");

			return 2;
		},
		1
	}
	},

	{
	"scene_load",
	{
		[](lua_State* L)
		{
			const char* path = luaL_checkstring(L, 1);

			std::string fileContents = FileRW::ReadFile(path);

			bool deserializeSuccess = true;
			std::vector<GameObjectRef> rootNodes = SceneFormat::Deserialize(fileContents, &deserializeSuccess);

			if (!deserializeSuccess)
				luaL_errorL(L, "%s", SceneFormat::GetLastErrorString().c_str());

			std::vector<Reflection::GenericValue> convertedArray;

			for (GameObject* node : rootNodes)
				convertedArray.push_back(node->ToGenericValue());

			Reflection::GenericValue gv = convertedArray;

			ScriptEngine::L::PushGenericValue(L, gv);

			return 1;
		},
		1
	}
	},

	{
		"imgui_begin",
		{
			[](lua_State* L)
			{
				const char* title = luaL_checkstring(L, 1);
				lua_pushboolean(L, ImGui::Begin(title));

				return 1;
			},
			1
		}
	},

	{
		"imgui_end",
		{
			[](lua_State*)
			{
				ImGui::End();

				return 0;
			},
			0
		}
	},

	{
		"imgui_setitemtooltip",
		{
			[](lua_State* L)
			{
				ImGui::SetItemTooltip("%s", luaL_checkstring(L, 1));

				return 0;
			},
			1
		}
	},

	{
		"imgui_item_hovered",
		{
			[](lua_State* L)
			{
				lua_pushboolean(L, ImGui::IsItemHovered());
				return 1;
			},
			0
		}
	},

	{
		"imgui_item_clicked",
		{
			[](lua_State* L)
			{
				lua_pushboolean(L, ImGui::IsItemClicked());
				return 1;
			},
			0
		}
	},

	{
		"imgui_text",
		{
			[](lua_State* L)
			{
				ImGui::TextUnformatted(luaL_checkstring(L, 1));
				return 0;
			},
			1
		}
	},

	{
		"imgui_image",
		{
			[](lua_State* L)
			{
				TextureManager* texManager = TextureManager::Get();
				uint32_t resId = texManager->LoadTextureFromPath(luaL_checkstring(L, 1));
				const Texture& texture = texManager->GetTextureResource(resId);

				ImGui::Image(
					texture.GpuId,
					ImVec2(
						static_cast<float>(luaL_optnumber(L, 2, texture.Width)),
						static_cast<float>(luaL_optnumber(L, 3, texture.Height))
					)
				);

				return 0;
			},
			3
		}
	},

	{
		"imgui_indent",
		{
			[](lua_State* L)
			{
				ImGui::Indent(static_cast<float>(luaL_checknumber(L, 1)));
				return 0;
			},
			1
		}
	},

	{
		"imgui_input_text",
		{
			[](lua_State* L)
			{
				const char* name = luaL_checkstring(L, 1);
				std::string value = luaL_checkstring(L, 2);
				ImGui::InputText(name, &value);

				lua_pushstring(L, value.c_str());

				return 1;
			},
			2
		}
	},

	{
		"imgui_input_float",
		{
			[](lua_State* L)
			{
				const char* title = luaL_checkstring(L, 1);
				float value = static_cast<float>(luaL_checknumber(L, 2));
				ImGui::InputFloat(title, &value);

				lua_pushnumber(L, value);

				return 1;
			}, 2
		}
	},

	{
		"imgui_button",
		{
			[](lua_State* L)
			{
				lua_pushboolean(L, ImGui::Button(luaL_checkstring(L, 1)));

				return 1;
			},
			1
		}
	},

	{
		"imgui_textlink",
		{
			[](lua_State* L)
			{
				lua_pushboolean(L, ImGui::TextLink(luaL_checkstring(L, 1)));

				return 1;
			},
			1
		}
	},

	{
		"imgui_checkbox",
		{
			[](lua_State* L)
			{
				const char* title = luaL_checkstring(L, 1);
				bool curval = luaL_checkboolean(L, 2);
				bool pressed = ImGui::Checkbox(title, &curval);

				lua_pushboolean(L, curval);
				lua_pushboolean(L, pressed);

				return 2;
			},
			2
		}
	},

	{
		"fd_inprogress",
		{
			[](lua_State* L)
			{
				lua_pushboolean(L, IsFdInProgress);
				return 1;
			},
			0
		}
	},

	{
	"fd_save",
	{
		[](lua_State* L)
		{
			if (IsFdInProgress)
				luaL_errorL(L, "The User has not completed the previous File Dialog");
			
			std::string defaultLocationCwd = luaL_optstring(L, 1, "");

			char* sdlcwd = SDL_GetCurrentDirectory();
			std::string defaultLocationAbs = sdlcwd + defaultLocationCwd;
			free(sdlcwd);

#ifdef _WIN32
			for (size_t i  = 0; i < defaultLocationAbs.size(); i++)
				if (defaultLocationAbs[i] == '/')
					defaultLocationAbs[i] = '\\';
#endif
			
			SDL_DialogFileFilter filter{};
			filter.name = luaL_optstring(L, 2, "All files");
			filter.pattern = luaL_optstring(L, 3, "*");

			// TODO a kind of hack to get what script we're running as?
			lua_getglobal(L, "script");
			Reflection::GenericValue script = ScriptEngine::L::LuaValueToGeneric(L, -1);
			GameObject* scriptObject = GameObject::FromGenericValue(script);
			// modules currently do not have a `script` global
			uint32_t scriptId = scriptObject ? scriptObject->ObjectId : PHX_GAMEOBJECT_NULL_ID;

			IsFdInProgress = true;
		
			SDL_ShowSaveFileDialog(
				fdCallback,
				NULL,
				SDL_GL_GetCurrentWindow(),
				&filter,
				1,
				defaultLocationAbs.c_str()
			);
		
			lua_pushthread(L);
		
			auto& b = s_YieldedCoroutines.emplace_back(
				L,
				// make sure the coroutine doesn't get de-alloc'd before we resume it
				lua_ref(L, -1),
				scriptId,
				YieldedCoroutine::ResumptionMode::Polled
			);
		
			b.RmPoll = [](std::vector<Reflection::GenericValue>* ReturnValues)
				-> bool
				{
					if (IsFdInProgress)
						return false;
					else
					{
						ReturnValues->emplace_back(FdResults[0]);
					
						return true;
					}
				};

			return lua_yield(L, 1);
		},
		0
	}
	},

	{
	"fd_open",
	{
		[](lua_State* L)
		{
			if (IsFdInProgress)
				luaL_errorL(L, "The User has not completed the previous File Dialog");

			std::string defaultLocationCwd = luaL_optstring(L, 1, "");

			char* sdlcwd = SDL_GetCurrentDirectory();
			std::string defaultLocationAbs = sdlcwd + defaultLocationCwd;
			free(sdlcwd);


#ifdef _WIN32
			for (size_t i  = 0; i < defaultLocationAbs.size(); i++)
				if (defaultLocationAbs[i] == '/')
					defaultLocationAbs[i] = '\\';
#endif

			SDL_DialogFileFilter filter{};
			filter.name = luaL_optstring(L, 2, "All files");
			filter.pattern = luaL_optstring(L, 3, "*");

			bool allowMultipleFiles = luaL_optboolean(L, 4, 0);

			// TODO a kind of hack to get what script we're running as?
			lua_getglobal(L, "script");
			Reflection::GenericValue script = ScriptEngine::L::LuaValueToGeneric(L, -1);
			GameObject* scriptObject = GameObject::FromGenericValue(script);
			// modules currently do not have a `script` global
			uint32_t scriptId = scriptObject ? scriptObject->ObjectId : PHX_GAMEOBJECT_NULL_ID;

			IsFdInProgress = true;
		
			SDL_ShowOpenFileDialog(
				fdCallback,
				NULL,
				SDL_GL_GetCurrentWindow(),
				&filter,
				1,
				defaultLocationAbs.c_str(),
				allowMultipleFiles
			);
		
			lua_pushthread(L);
		
			auto& b = ScriptEngine::s_YieldedCoroutines.emplace_back(
				L,
				// make sure the coroutine doesn't get de-alloc'd before we resume it
				lua_ref(L, -1),
				scriptId,
				YieldedCoroutine::ResumptionMode::Polled
			);
		
			b.RmPoll = [](std::vector<Reflection::GenericValue>* ReturnValues)
				-> bool
				{
					if (IsFdInProgress)
						return false;
					else
					{
						std::vector<Reflection::GenericValue> pathsArray;
						pathsArray.reserve(FdResults.size());
					
						for (const std::string& r : FdResults)
							pathsArray.emplace_back(r);
					
						*ReturnValues = { pathsArray };
					
						return true;
					}
				};

			return lua_yield(L, 1);
		},
		0
	}
	},

	{
	"file_read",
	{
		[](lua_State* L)
		{
			const char* path = luaL_checkstring(L, 1);

			lua_pushstring(L, FileRW::ReadFile(path).c_str());

			return 1;
		},
		1
	}
	},

	{
	"file_write",
	{
		[](lua_State* L)
		{
			const char* path = luaL_checkstring(L, 1);
			const char* contents = luaL_checkstring(L, 2);
			bool prependResDir = luaL_checkboolean(L, 3);

			FileRW::WriteFile(path, contents, prependResDir);

			return 0;
		},
		3
	}
	},

	{
	"file_write_rcd",
	{
		[](lua_State* L)
		{
			const char* path = luaL_checkstring(L, 1);
			const char* contents = luaL_checkstring(L, 2);
			bool prependResDir = luaL_checkboolean(L, 3);

			FileRW::WriteFileCreateDirectories(path, contents, prependResDir);

			return 0;
		},
		3
	}
	},

	{
	"conf_get",
	{
		[](lua_State* L)
		{
			const char* k = luaL_checkstring(L, 1);
			nlohmann::json v = EngineJsonConfig[k];
			pushJson(L, v);

			return 1;
		},
		1
	}
	},

	{
	"json_parse",
	{
		[](lua_State* L)
		{
			const char* jsonStr = luaL_checkstring(L, 1);
			nlohmann::json json = nlohmann::json::parse(jsonStr);
			pushJson(L, json);

			return 1;
		},
		1
	}
	},

	{
	"json_dump",
	{
		[](lua_State* L)
		{
			luaL_checktype(L, 1, LUA_TTABLE);
			int indent = luaL_optinteger(L, 2, 2);

			nlohmann::json json{};
			luaTableToJson(L, json);

			lua_pushstring(L, json.dump(indent).c_str());

			return 1;
		},
		1
	}
	},

	{
		"",
		{
			nullptr,
			0
		}
	}
};

std::pair<std::string_view, GlobalFn>* ScriptEngine::L::GlobalFunctions = s_GlobalFunctions;
