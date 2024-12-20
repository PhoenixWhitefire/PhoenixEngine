#include <glm/mat4x4.hpp>
#include <luau/VM/src/lstate.h>
#include <luau/VM/include/lualib.h>

#include "gameobject/ScriptEngine.hpp"
#include "Debug.hpp"

#include "IntersectionLib.hpp"
#include "datatype/Vector3.hpp"
#include "datatype/Color.hpp"
#include "datatype/GameObject.hpp"
#include "asset/MeshProvider.hpp"
#include "asset/ModelLoader.hpp"
#include "asset/SceneFormat.hpp"
#include "UserInput.hpp"
#include "FileRW.hpp"

template <class T> static void throwWrapped(T exc)
{
	throw(exc);
}

bool ScriptEngine::s_BackendScriptWantGrabMouse = false;
std::unordered_map<lua_State*, std::shared_future<Reflection::GenericValue>> ScriptEngine::s_YieldedCoroutines{};
const std::unordered_map<Reflection::ValueType, lua_Type> ScriptEngine::ReflectedTypeLuaEquivalent =
{
		{ Reflection::ValueType::Null,        lua_Type::LUA_TNIL       },

		{ Reflection::ValueType::Bool,        lua_Type::LUA_TBOOLEAN   },
		{ Reflection::ValueType::Integer,     lua_Type::LUA_TNUMBER    },
		{ Reflection::ValueType::Double,      lua_Type::LUA_TNUMBER    },
		{ Reflection::ValueType::String,      lua_Type::LUA_TSTRING    },

		{ Reflection::ValueType::Color,       lua_Type::LUA_TUSERDATA  },
		{ Reflection::ValueType::Vector3,     lua_Type::LUA_TUSERDATA  },
		{ Reflection::ValueType::Matrix,      lua_Type::LUA_TUSERDATA  },

		{ Reflection::ValueType::GameObject,  lua_Type::LUA_TUSERDATA  },

		{ Reflection::ValueType::Array,       lua_Type::LUA_TTABLE     },
		{ Reflection::ValueType::Map,         lua_Type::LUA_TTABLE     }
};

static void pushVector3(lua_State* L, const Vector3& vec)
{
	void* ptrTovec = lua_newuserdata(L, sizeof(Vector3));
	*(Vector3*)ptrTovec = vec;

	luaL_getmetatable(L, "Vector3");
	lua_setmetatable(L, -2);
}

static void pushColor(lua_State* L, const Color& col)
{
	void* ptr = lua_newuserdata(L, sizeof(Vector3));
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

static void pushGameObject(lua_State* L, GameObject* obj)
{
	uint32_t* ptrToObj = (uint32_t*)lua_newuserdata(L, sizeof(uint32_t));
	*ptrToObj = obj ? obj->ObjectId : 0;

	luaL_getmetatable(L, "GameObject");
	lua_setmetatable(L, -2);
}

Reflection::GenericValue ScriptEngine::L::LuaValueToGeneric(lua_State* L, int StackIndex)
{
	switch (lua_type(L, StackIndex))
	{
	case (lua_Type::LUA_TNIL):
	{
		return Reflection::GenericValue();
	}
	case (lua_Type::LUA_TBOOLEAN):
	{
		return (bool)lua_toboolean(L, StackIndex);
	}
	case (lua_Type::LUA_TNUMBER):
	{
		return lua_tonumber(L, StackIndex);
	}
	case (lua_Type::LUA_TSTRING):
	{
		return lua_tostring(L, StackIndex);
	}
	case (lua_Type::LUA_TUSERDATA):
	{
		// IMPORTANT!!
		// Requires `LuauPreserveLudataRenaming` to be enabled in `Luau/VM/src/ltm.cpp`
		// 11/09/2024
		const char* tname = luaL_typename(L, StackIndex);

		if (strcmp(tname, "Vector3") == 0)
		{
			Vector3 vec = *(Vector3*)lua_touserdata(L, StackIndex);
			return vec.ToGenericValue();
		}
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
			luaL_error(L, std::vformat(
				"Couldn't convert a '{}' userdata to a GenericValue (unrecognized)",
				std::make_format_args(tname)
			).c_str());
	}
	case (lua_Type::LUA_TTABLE):
	{
		// 15/09/2024
		// TODO
		// Maps

		std::vector<Reflection::GenericValue> items;

		// https://www.lua.org/manual/5.1/manual.html#lua_next
		lua_pushnil(L);

		while (lua_next(L, StackIndex - 1) != 0)
		{
			items.push_back(ScriptEngine::L::LuaValueToGeneric(L, -1));
			lua_pop(L, 1);
		}

		return items;
	}
	default:
	{
		const char* tname = luaL_typename(L, StackIndex);
		luaL_error(L, std::vformat(
			"Could not convert type {} to a GenericValue (no conversion case)",
			std::make_format_args(tname)).c_str()
		);
	}
	}
}

void ScriptEngine::L::PushGenericValue(lua_State* L, const Reflection::GenericValue& gv)
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
		lua_pushstring(L, gv.AsString().c_str());
		break;
	}
	case (Reflection::ValueType::Vector3):
	{
		pushVector3(L, gv);
		break;
	}
	case (Reflection::ValueType::Color):
	{
		pushColor(L, gv);
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

		std::vector<Reflection::GenericValue> array = gv.AsArray();

		for (int index = 0; index < array.size(); index++)
		{
			lua_pushinteger(L, index);
			L::PushGenericValue(L, array[index]);
			lua_settable(L, -3);
		}

		break;
	}
	case (Reflection::ValueType::Map):
	{
		std::vector<Reflection::GenericValue> array = gv.AsArray();

		if (array.size() % 2 != 0)
			throw("GenericValue type was Map, but it does not have an even number of elements!");

		lua_newtable(L);

		for (int index = 0; index < array.size(); index++)
		{
			L::PushGenericValue(L, array[index]);

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

void ScriptEngine::L::PushGameObject(lua_State* L, GameObject* Object)
{
	auto gv = Reflection::GenericValue(Object->ObjectId);
	gv.Type = Reflection::ValueType::GameObject;

	PushGenericValue(L, gv);
}

void ScriptEngine::L::PushFunction(lua_State* L, const char* Name)
{
	// need to remember our name
	lua_pushstring(L, Name);

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

				for (int arg = 1; arg < numArgs + 1; arg++)
					argsString += std::string(luaL_typename(L, -(numArgs + 1 - arg))) + ", ";

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

				auto expectedLuaTypeIt = ScriptEngine::ReflectedTypeLuaEquivalent.find(paramType);

				if (expectedLuaTypeIt == ScriptEngine::ReflectedTypeLuaEquivalent.end())
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
					inputs.push_back(L::LuaValueToGeneric(L, argStackIndex));
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
				L::PushGenericValue(L, output);

			return (int)func.Outputs.size();

			// ... kinda expected more, but ngl i feel SOOOO gigabrain for
			// giving ::GenericValue an Array, like, it all just clicks in now!
			// And then Maps just being Arrays, except odd elements are the keys
			// and even elements are the values?! Call me Einstein already on god-
			// (Me writing this as Rendering is completely busted and I have no clue
			// why oh no
			// 15/08/2024
		},

		Name,
		1
	);
}

std::unordered_map<std::string, lua_CFunction> ScriptEngine::L::GlobalFunctions =
{
	{
	"matrix_getv",
	[](lua_State* L)
	{
		glm::mat4& mtx = *(glm::mat4*)luaL_checkudata(L, 1, "Matrix");
		int r = luaL_checkinteger(L, 2);
		int c = luaL_checkinteger(L, 3);

		lua_pushnumber(L, mtx[r][c]);

		return 1;
	}
	},

	{
	"matrix_setv",
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
	}
	},

	{
	"input_keypressed",
	[](lua_State* L)
	{
		if (UserInput::InputBeingSunk)
			lua_pushboolean(L, false);
		else
		{
			const char* kname = luaL_checkstring(L, 1);
			lua_pushboolean(L, UserInput::IsKeyDown(SDL_KeyCode(kname[0])));
		}

		return 1;
	}
	},

	{
	"input_mouse_bdown",
	[](lua_State* L)
	{
		if (UserInput::InputBeingSunk)
			lua_pushboolean(L, false);
		else
		{
			bool lmb = luaL_checkboolean(L, 1);
			lua_pushboolean(L, UserInput::IsMouseButtonDown(lmb));
		}

		return 1;
	}
	},

	{
	"input_mouse_getpos",
	[](lua_State* L)
	{
		int mx = 0;
		int my = 0;

		SDL_GetMouseState(&mx, &my);

		lua_pushinteger(L, mx);
		lua_pushinteger(L, my);

		return 2;
	}
	},

	{
	"input_mouse_setlocked",
	[](lua_State* L)
	{
		s_BackendScriptWantGrabMouse = luaL_checkboolean(L, 1);
		return 0;
	}
	},

	{
		"sleep",
		[](lua_State* L)
		{
			int sleepTime = luaL_checkinteger(L, 1);

			lua_pushnil(L);

			auto a = std::async(
				std::launch::async,
				[](int st)
				{
					std::this_thread::sleep_for(std::chrono::seconds(st));
					return Reflection::GenericValue(st);
				},
				sleepTime
			);
			ScriptEngine::s_YieldedCoroutines.insert(std::pair(L, a.share()));

			return lua_yield(L, 1);
		}
	},

	{
	"require",
	// `lua_require` from `Luau/CLI/Repl.cpp` 18/09/2024
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
	}
	},

	{
		"mesh_get",
		[](lua_State* L)
		{
			const char* meshPath = luaL_checkstring(L, 1);

			MeshProvider* meshProvider = MeshProvider::Get();

			uint32_t meshId = meshProvider->LoadFromPath(meshPath, false);
			Mesh* mesh = meshProvider->GetMeshResource(meshId);

			lua_newtable(L);

			lua_newtable(L);

			for (size_t vi = 0; vi < mesh->Vertices.size(); vi++)
			{
				const Vertex& v = mesh->Vertices.at(vi);

				lua_pushinteger(L, static_cast<int32_t>(vi + 1));
				lua_newtable(L);

				pushVector3(L, v.Position);
				lua_setfield(L, -2, "Position");

				pushVector3(L, v.Normal);
				lua_setfield(L, -2, "Normal");

				pushVector3(L, v.Color);
				lua_setfield(L, -2, "Color");

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

			for (uint32_t indexIndex = 0; indexIndex < mesh->Indices.size(); indexIndex++)
			{
				lua_pushinteger(L, indexIndex + 1);
				lua_pushinteger(L, static_cast<int32_t>(mesh->Indices.at(indexIndex)));
				lua_settable(L, -3);
			}

			lua_setfield(L, -2, "Indices");

			return 1;
	}
	},

	{
		"mesh_set",
		[](lua_State* L)
		{
			const char* meshName = luaL_checkstring(L, 1);

			Mesh mesh;

			lua_getfield(L, -1, "Vertices");

			lua_pushnil(L);
			while (lua_next(L, -2) != 0)
			{
				lua_getfield(L, -1, "Position");
				glm::vec3 position = Vector3(ScriptEngine::L::LuaValueToGeneric(L));

				lua_getfield(L, -2, "Normal");
				glm::vec3 normal = Vector3(ScriptEngine::L::LuaValueToGeneric(L));

				lua_getfield(L, -3, "Color");
				glm::vec3 color = Vector3(ScriptEngine::L::LuaValueToGeneric(L));
				
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

				mesh.Vertices.emplace_back(Vertex{ position, normal, color, uv });

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
			meshProvider->Assign(mesh, meshName);

			// Informs the Renderer the mesh doesn't exist on the GPU on it's own
			// and must be uploaded when it's rendered
			meshProvider->GetMeshResource(meshProvider->LoadFromPath(meshName))->GpuId = UINT32_MAX;

			return 0;
		}
	},

	{
		"world_raycast",
		[](lua_State* L)
		{
			GameObject* workspace = GameObject::s_DataModel->GetChildOfClass("Workspace");

			if (!workspace)
				luaL_error(L, "A Workspace was not found within the DataModel");

			glm::vec3 origin = Vector3(LuaValueToGeneric(L, -3));
			glm::vec3 vector = Vector3(LuaValueToGeneric(L, -2));
			std::vector<Reflection::GenericValue> providedIgnoreList = LuaValueToGeneric(L, -1).AsArray();

			std::vector<GameObject*> ignoreList;
			for (const Reflection::GenericValue& gv : providedIgnoreList)
				ignoreList.push_back(GameObject::GetObjectById(static_cast<uint32_t>((uint64_t)gv.Value)));

			IntersectionLib::Intersection result;
			GameObject* hitObject = nullptr;
			double closestHit = INFINITY;

			for (GameObject* p : workspace->GetDescendants())
			{
				if (std::find(ignoreList.begin(), ignoreList.end(), p) != ignoreList.end())
					continue;

				Object_Base3D* object = dynamic_cast<Object_Base3D*>(p);

				if (object)
				{
					glm::vec3 pos = object->Transform[3];
					glm::vec3 size = object->Size;

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
							hitObject = object;
						}
				}
			}

			if (hitObject)
			{
				lua_newtable(L);

				ScriptEngine::L::PushGameObject(L, hitObject);
				lua_setfield(L, -2, "Object");

				Reflection::GenericValue posg = Vector3(result.Vector).ToGenericValue();

				ScriptEngine::L::PushGenericValue(L, posg);
				lua_setfield(L, -2, "Position");

				Reflection::GenericValue normalg = Vector3(result.Normal).ToGenericValue();

				ScriptEngine::L::PushGenericValue(L, normalg);
				lua_setfield(L, -2, "Normal");
			}
			else
				lua_pushnil(L);

			return 1;
		}
	},

	{
		"model_import",
		[](lua_State* L)
		{
			const char* path = luaL_checkstring(L, 1);

			const std::vector<GameObject*>& loadedRoots = ModelLoader(path, nullptr).LoadedObjs;
			
			lua_newtable(L);

			for (int index = 0; index < loadedRoots.size(); index++)
			{
				GameObject* object = loadedRoots[index];
				Reflection::GenericValue gv = object->ToGenericValue();

				lua_pushinteger(L, index);
				ScriptEngine::L::PushGenericValue(L, gv);

				lua_settable(L, -3);
			}

			return 1;
		}
	},

	{
		"scene_save",
		[](lua_State* L)
		{
			Reflection::GenericValue rootNodesGv = ScriptEngine::L::LuaValueToGeneric(L, -2);
			const char* path = luaL_checkstring(L, 2);

			std::vector<Reflection::GenericValue> rootNodesArray = rootNodesGv.AsArray();
			std::vector<GameObject*> rootNodes;

			for (const Reflection::GenericValue& gv : rootNodesArray)
				rootNodes.push_back(GameObject::FromGenericValue(gv));

			std::string fileContents = SceneFormat::Serialize(rootNodes, path);
			FileRW::WriteFileCreateDirectories(path, fileContents, true);

			lua_pushboolean(L, 1);
			lua_pushstring(L, "No errors occurred");

			return 2;
		}
	},

	{
		"scene_load",
		[](lua_State* L)
		{
			const char* path = luaL_checkstring(L, 1);

			std::string fileContents = FileRW::ReadFile(path);

			bool deserializeSuccess = true;
			std::vector<GameObject*> rootNodes = SceneFormat::Deserialize(fileContents, &deserializeSuccess);

			if (!deserializeSuccess)
				luaL_errorL(L, SceneFormat::GetLastErrorString().c_str());

			std::vector<Reflection::GenericValue> convertedArray;

			for (GameObject* node : rootNodes)
				convertedArray.push_back(node->ToGenericValue());

			Reflection::GenericValue gv = convertedArray;

			ScriptEngine::L::PushGenericValue(L, gv);

			return 1;
		}
	}
};
