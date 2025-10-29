#include <luau/Require/Runtime/include/Luau/Require.h>
#include <luau/VM/include/lualib.h>
#include <luau/VM/src/lstate.h>
#include <Luau/Compiler.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

#include <tracy/Tracy.hpp>

#include "script/ScriptEngine.hpp"
#include "script/luhx.hpp"
#include "datatype/Color.hpp"
#include "GlobalJsonConfig.hpp"
#include "Engine.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

std::vector<ScriptEngine::YieldedCoroutine> ScriptEngine::s_YieldedCoroutines{};
const std::unordered_map<Reflection::ValueType, lua_Type> ScriptEngine::ValueTypeToLuauType =
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
		{ Reflection::ValueType::Function,    lua_Type::LUA_TFUNCTION  },

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

void ScriptEngine::L::PushJson(lua_State* L, const nlohmann::json& v)
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
			PushJson(L, v[i]);
			lua_settable(L, -3);
		}
		
		break;
	}
	case nlohmann::json::value_t::object:
	{
		lua_newtable(L);

		for (auto it = v.begin(); it != v.end(); ++it)
		{
			PushJson(L, it.value());
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

#define ERROR_CONTEXTUALIZED(e, ...) { \
if (Context.size() > 0) \
	luaL_error(L, e " in %s", __VA_ARGS__, Context.c_str()); \
else \
	luaL_error(L, e, __VA_ARGS__); } \

nlohmann::json ScriptEngine::L::ToJson(lua_State* L, int StackIndex, std::string Context)
{
	switch (lua_type(L, StackIndex))
	{
	case LUA_TNIL:
	{
		return {};
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
		nlohmann::json str = luaL_tolstring(L, StackIndex, nullptr);
		lua_pop(L, 1);
		return str;
	}
	case LUA_TTABLE:
	{
		nlohmann::json t = nlohmann::json::object();
		int keytype = LUA_TNIL;

		luaL_checkstack(L, 5, "JSON encode");
		lua_pushvalue(L, StackIndex);
		lua_pushnil(L);

		while (lua_next(L, -2) != 0)
		{
			if (lua_type(L, -2) != keytype && keytype != LUA_TNIL)
			{
				// C++ does not specify the order of evaluation of function arguments,
				// and `luaL_tolstring` will produce side-effects (pushing string on stack)
				const char* ktname = luaL_typename(L, -2);

				ERROR_CONTEXTUALIZED(
					"All keys must have the same type. Previous type: %s, Current type: %s ('%s')",
					lua_typename(L, keytype), ktname, luaL_tolstring(L, -2, nullptr)
				);
			}

			if (keytype == LUA_TNIL)
			{
				keytype = lua_type(L, -2);

				if (keytype != LUA_TSTRING && keytype != LUA_TNUMBER)
				{
					const char* ktname = luaL_typename(L, -2);

					ERROR_CONTEXTUALIZED(
						"Table keys expected to be string or number, got '%s' (%s)",
						luaL_tolstring(L, -2, nullptr), ktname
					); // `luaL_tolstring` always pushes the string onto the stack,
					// which is why the succeeding arguments are offset by -1
				}

				if (keytype == LUA_TNUMBER)
					t = nlohmann::json::array();
			}

			if (lua_type(L, -2) == LUA_TNUMBER)
			{
				int index = lua_tointeger(L, -2);

				if (index == 0)
				{
					const char* vtname = luaL_typename(L, -1);

					ERROR_CONTEXTUALIZED(
						"Tables cannot be zero-indexed. Value: '%s' (%s)",
						luaL_tolstring(L, -1, nullptr), vtname
					);
				}

				if (index < 0)
				{
					const char* vtname = luaL_typename(L, -1);

					ERROR_CONTEXTUALIZED(
						"Tables cannot have negative indices. Index: %i, Value: '%s' (%s)",
						index, luaL_tolstring(L, -1, nullptr), vtname
					);
				}

				if (Context.size() == 0)
					Context = "Array";

				t[index - 1] = L::ToJson(L, -1, Context + "[" + std::to_string(index) + "]");
			}
			else
			{
				assert(lua_type(L, -2) == LUA_TSTRING);
				const char* key = luaL_checkstring(L, -2);

				if (Context.size() == 0)
					Context = "Object";

				t[key] = L::ToJson(L, -1, Context + "." + key);
			}

			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		return t;
	}

	[[unlikely]] default:
	{
		const char* vtname = luaL_typename(L, StackIndex);
		const char* vstr = luaL_tolstring(L, StackIndex, nullptr);

		ERROR_CONTEXTUALIZED(
			"Cannot convert '%s' (%s) to a JSON value",
			vstr, vtname
		);
	}
	}
}
#undef ERROR_CONTEXTUALIZED

int ScriptEngine::CompileAndLoad(lua_State* L, const std::string& SourceCode, const std::string& ChunkName)
{
	ZoneScoped;
	ZoneText(ChunkName.data(), ChunkName.size());
	
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
	compileOptions.debugLevel = L::DebugBreak ? 2 : 1;
	compileOptions.mutableGlobals = mutableGlobals;

	std::string bytecode = Luau::compile(SourceCode, compileOptions);

	int result = luau_load(L, ChunkName.c_str(), bytecode.data(), bytecode.size(), 0);

	if (result == 0)
	{
		lua_pushlstring(L, ChunkName.data(), ChunkName.size());
		lua_setglobal(L, "_CHUNKNAME");
	}

	return result;
}

#define YIELDBLOCKERTRACKING "_YIELDBLOCKERS"

Reflection::GenericValue ScriptEngine::L::ToGeneric(lua_State* L, int StackIndex)
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
		Reflection::GenericValue str = luaL_tolstring(L, StackIndex, nullptr);
		lua_pop(L, 1);
		return str;
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
	case LUA_TFUNCTION:
	{
		Reflection::GenericValue gv;
		gv.Type = Reflection::ValueType::Function;

		lua_State* CL = lua_newthread(L);
		lua_pushvalue(L, StackIndex);
		lua_xmove(L, CL, 1);

		std::string fndbinfo;
		lua_Debug ar;
		lua_getinfo(L, 0, "n", &ar);
		std::string fnname = ar.name;
		if (fnname == "__namecall")
			fnname = L->namecall->data;

		lua_getinfo(L, 1, "sln", &ar);
		fndbinfo = std::format(
			"{}:{} to {} in {}",
			ar.short_src, ar.currentline, fnname, ar.name ? ar.name : "<anonymous>"
		);

		gv.Val.Func = new Reflection::GenericFunction([CL, fndbinfo](const std::vector<Reflection::GenericValue>& Inputs)
			-> std::vector<Reflection::GenericValue>
			{
				lua_pushvalue(CL, -1); // keep the function value

				for (const Reflection::GenericValue& i : Inputs)
					PushGenericValue(CL, i);
				
				lua_getglobal(CL, YIELDBLOCKERTRACKING);
				if (!lua_istable(CL, -1))
				{
					lua_pop(CL, 1); // pop nil value
					lua_newtable(CL);
					lua_pushvalue(CL, -1); // B
					lua_setglobal(CL, YIELDBLOCKERTRACKING); // leaves same empty table at stack top after popping B
				}

				int ybsize = lua_objlen(CL, -1);
				lua_pushinteger(CL, ybsize + 1);
				lua_pushstring(CL, fndbinfo.c_str());
				lua_settable(CL, -3);
				lua_pop(CL, 1);

				int status = lua_pcall(CL, (int)Inputs.size(), -1, 0);

				lua_getglobal(CL, YIELDBLOCKERTRACKING);
				luaL_checktype(CL, -1, LUA_TTABLE);

				lua_pushinteger(CL, ybsize + 1);
				lua_pushnil(CL);
				lua_settable(CL, -3);
				lua_pop(CL, 1);

				if (status == LUA_OK)
				{
					std::vector<Reflection::GenericValue> retvals;
					for (int i = 2; i < lua_gettop(CL); i++)
						retvals.push_back(L::ToGeneric(CL, i));

					return retvals;
				}

				RAISE_RT(luaL_checkstring(CL, -1));
			});

		return gv;
	}
	case LUA_TTABLE:
	{
		// 15/09/2024
		// TODO
		// Maps

		std::vector<Reflection::GenericValue> items;

		lua_pushvalue(L, StackIndex);

		// https://www.lua.org/manual/5.1/manual.html#lua_next
		lua_pushnil(L);

		while (lua_next(L, -2) != 0)
		{
			items.push_back(L::ToGeneric(L, -1));
			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		return items;
	}
	default:
	{
		const char* tname = luaL_typename(L, StackIndex);
		luaL_error(L, "Could not convert type '%s' to a GenericValue (no conversion case)", tname);
	}
	}
}

void ScriptEngine::L::CheckType(lua_State* L, Reflection::ValueType Type, int StackIndex)
{
	ZoneScoped;

	bool isOptional = (uint8_t)Type & (uint8_t)Reflection::ValueType::Null;
	int givenType = lua_type(L, StackIndex);

	if (!isOptional || givenType != LUA_TNIL)
	{
		Type = (Reflection::ValueType)((uint8_t)Type & ~(uint8_t)Reflection::ValueType::Null);
		const auto& reflToLuaIt = ValueTypeToLuauType.find(Type);

		if (reflToLuaIt == ValueTypeToLuauType.end())
			luaL_error(L,
				"No defined mapping between a '%s' and a Luau type",
				Reflection::TypeAsString(Type).c_str()
			);

		// the literal `if` check inside this function likes to take 190 microseconds in Debug mode for some reason
		// probably some cache bullshit
		// fuck
		luaL_checktype(L, StackIndex, reflToLuaIt->second);

		if (reflToLuaIt->second == LUA_TUSERDATA)
			luaL_checkudata(L, StackIndex, Reflection::TypeAsString(Type).c_str());
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
		std::span<Reflection::GenericValue> array = std::span((Reflection::GenericValue*)gv.Val.Ptr, gv.Size);

		if (array.size() % 2 != 0)
			RAISE_RT("GenericValue type was Map, but it does not have an even number of elements!");

		lua_createtable(L, 0, array.size() / 2);

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
		std::string_view typeName = Reflection::TypeAsString(gv.Type);
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
		/*uint32_t* ptrToObj = (uint32_t*)lua_newuserdatadtor(
			L,
			sizeof(uint32_t),
			[](void* ptrId)
			{
				uint32_t id = *(uint32_t*)ptrId;

				if (GameObject* o = GameObject::GetObjectById(id))
					o->DecrementHardRefs(); // removes the reference in `::Create`
			}
		);*/
		uint32_t* ptrToObj = (uint32_t*)lua_newuserdata(L, sizeof(uint32_t));
		*ptrToObj = obj ? obj->ObjectId : PHX_GAMEOBJECT_NULL_ID;
		
		luaL_getmetatable(L, "GameObject");
		lua_setmetatable(L, -2);
	}
}

int ScriptEngine::L::HandleMethodCall(
	lua_State* L,
	const Reflection::MethodDescriptor* func,
	ReflectorRef Reflector
)
{
	const std::vector<Reflection::ValueType>& paramTypes = func->Inputs;
	int numArgs = lua_gettop(L) - 1;
	assert(numArgs >= 0);
	// missing parameter declarations?
	assert(paramTypes.size() >= static_cast<size_t>(numArgs));

	int numParams = static_cast<int32_t>(paramTypes.size());
	int minArgs = 0;

	for (int i = 0; i < numParams; i++)
	{
		const Reflection::ValueType& param = paramTypes[i];

		if (!((uint8_t)param & (uint8_t)Reflection::ValueType::Null))
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
		Log::WarningF("Function received {} more arguments than necessary",
			numArgs - numParams
		);
	}

	std::vector<Reflection::GenericValue> inputs;

	// This *entire* for-loop is just for handling input arguments
	for (int index = 0; index < numArgs; index++)
	{
		Reflection::ValueType paramType = paramTypes[index];

		assert(luaL_checkudata(L, 1, "GameObject"));

		// offset the stack so the error message gives the correct argument number
		// without this, the first argument would be reported as "argument #2"
		L->base++;
		ScriptEngine::L::CheckType(L, paramType, index + 1);
		L->base--;
		inputs.push_back(L::ToGeneric(L, index + 2));
	}

	// Now, onto the *REAL* business...
	std::vector<Reflection::GenericValue> outputs;

	try
	{
		outputs = func->Func(Reflector.Referred(), inputs);
	}
	catch (const std::runtime_error& err)
	{
		luaL_error(L, "%s", err.what());
	}

	assert(outputs.size() == func->Outputs.size());

	for (size_t i = 0; i < outputs.size(); i++)
	{
		const Reflection::GenericValue& output = outputs[i];

#ifndef NDEBUG
		Reflection::ValueType base = func->Outputs[i];
		if ((uint8_t)base > (uint8_t)Reflection::ValueType::Null)
			base = (Reflection::ValueType)((uint8_t)base - (uint8_t)Reflection::ValueType::Null);

		assert(output.Type == base
			|| ((uint8_t)func->Outputs[i] > (uint8_t)Reflection::ValueType::Null && output.Type == Reflection::ValueType::Null)
		);
#endif

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

#include <imgui_internal.h> // needed for `ImGuiContext`

void ScriptEngine::L::Yield(lua_State* L, int NumResults, std::function<void(YieldedCoroutine&)> Configure)
{
	if (ImGuiContext* ctx = ImGui::GetCurrentContext(); ctx && ctx->CurrentWindowStack.Size > 1)
	{
		lua_Debug ar;
		lua_getinfo(L, 0, "n", &ar);
		RAISE_RTF(
			"Cannot yield with '{}' while in Dear ImGui section",
			ar.name ? ar.name : "<unknown>"
		);	
	}

	lua_getglobal(L, YIELDBLOCKERTRACKING);
	if (lua_istable(L, -1) && lua_objlen(L, -1) > 0)
	{
		std::vector<std::string> blockerslist;

		lua_pushnil(L);
		while (lua_next(L, -2) != 0)
		{
			blockerslist.push_back(luaL_checkstring(L, -1));
			lua_pop(L, 1);
		}

		std::string blockers;

		for (size_t i = blockerslist.size(); i != 0; i--)
		{
			blockers.append(blockerslist[i - 1]);
			blockers.append("\n");
		}

		lua_Debug ar;
		lua_Debug yieldar;
		lua_getinfo(L, 0, "n", &yieldar);
		lua_getinfo(L, 1, "sln", &ar);

		RAISE_RTF(
			"{}:{} in {}: Cannot yield right now with '{}', blocked by the following functions:\n{}",
			ar.short_src, ar.currentline, ar.name ? ar.name : "<anonymous>", yieldar.name ? yieldar.name : "<unknown>", blockers.c_str()
		);
	}

	if (L->nCcalls > L->baseCcalls)
	{
		// if a `lua_Exception` is thrown by `lua_yield`, we hit an assertion in
		// `ldo.cpp` line 137
		// LUAU_ASSERT(e.getThread() == L)
		lua_Debug ar;
		lua_getinfo(L, 0, "n", &ar);
		RAISE_RTF("Cannot yield with '{}' right now", ar.name ? ar.name : "<unknown>");
	}

	// TODO a kind of hack to get what script we're running as?
	lua_getglobal(L, "script");
	Reflection::GenericValue script = ScriptEngine::L::ToGeneric(L, -1);
	GameObject* scriptObject = GameObject::FromGenericValue(script);
	// modules currently do not have a `script` global
	uint32_t scriptId = scriptObject ? scriptObject->ObjectId : PHX_GAMEOBJECT_NULL_ID;
	// need to do that before `lua_yield` because of thread chicanery idk how it works

	lua_yield(L, NumResults);
	lua_pushthread(L);

	YieldedCoroutine& yc = s_YieldedCoroutines.emplace_back(
		L,
		// make sure the coroutine doesn't get de-alloc'd before we resume it
		lua_ref(L, -1),
		scriptId,
		YieldedCoroutine::ResumptionMode::INVALID
	);

	Configure(yc);
	assert(yc.Mode != YieldedCoroutine::ResumptionMode::INVALID);
}

void ScriptEngine::L::PushMethod(lua_State* L, const Reflection::MethodDescriptor* Method, ReflectorRef Reflector)
{
	// if we dont do this then comparison will not work
	// ex: `game.Close == game.Close`

	lua_pushlightuserdata(L, const_cast<Reflection::MethodDescriptor*>(Method));
	lua_rawget(L, LUA_ENVIRONINDEX);

	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1); // remove `nil`, stack empty

		static_assert(sizeof(Reflector) <= sizeof(void*));
		void* data = nullptr;
		memcpy(&data, &Reflector, sizeof(Reflector));

		lua_pushlightuserdata(L, const_cast<Reflection::MethodDescriptor*>(Method));
		lua_pushlightuserdata(L, data);

		lua_pushcclosure(
			L,
			[](lua_State* L)
			{
				Reflection::MethodDescriptor* fn =
					static_cast<Reflection::MethodDescriptor*>(lua_tolightuserdata(L, lua_upvalueindex(1)));
				auto& fc = *static_cast<decltype(Reflector)*>(lua_tolightuserdata(L, lua_upvalueindex(2)));

				return ScriptEngine::L::HandleMethodCall(
					L,
					fn,
					fc
				);
			},

			"PushMethodThunk",
			1
		); // stack is now just closure

		lua_pushlightuserdata(L, const_cast<Reflection::MethodDescriptor*>(Method));  // stack: closure, lud
		lua_pushvalue(L, -2);                                                 // stack: closure, lud, closure
		lua_settable(L, LUA_ENVIRONINDEX);                                    // map closure (value) to lud (key)

		// stack is now just closure
	}
	// value we fetch from `_ENVIRON` will be closure that was pushed earlier
}

// modified version of `db_traceback` from `VM/src/ldblib.cpp`
void ScriptEngine::L::DumpStacktrace(
	lua_State* L,
	std::string* Into,
	int Level,
	const char* Message
)
{
	lua_Debug ar;

	if (Message)
	{
		if (Into)
		{
			Into->append(Message);
			Into->append("\n");
		}
		else
			Log::Append(Message);
	}
	
	for (int i = Level; lua_getinfo(L, i, "sln", &ar); i++)
	{
		std::string line = "from ";

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

			if (i == 0 && strcmp(ar.name, "__namecall") == 0)
				line.append(L->namecall->data);
			else
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

struct EventSignalData
{
	ReflectorRef Reflector;
	const Reflection::EventDescriptor* Event = nullptr;
	const char* EventName = nullptr;
};

struct EventConnectionData
{
	ReflectorRef Reflector;
	ObjectHandle Script;
	const Reflection::EventDescriptor* Event = nullptr;
	lua_State* L = nullptr;
	uint32_t ConnectionId = UINT32_MAX;
	int ThreadRef = LUA_NOREF;
	int SignalRef = LUA_NOREF;
	bool CallbackYields = false;
};

static void pushSignal(
	lua_State* L,
	const Reflection::EventDescriptor* Event,
	const ReflectorRef& Reflector,
	const char* EventName
)
{
	EventSignalData* ev = (EventSignalData*)lua_newuserdata(L, sizeof(EventSignalData));
	ev->Reflector = Reflector;
	ev->EventName = EventName;
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

	GameObject* obj = GameObject::FromGenericValue(ScriptEngine::L::ToGeneric(L, 1));
	const char* key = luaL_checkstring(L, 2);

	ZoneText(key, strlen(key));

	if (strcmp(key, "Exists") == 0)
	{
		// whether or not it exists
		lua_pushboolean(L, obj ? 1 : 0);
		return 1;
	}

	LUA_ASSERT(obj, "Tried to index '%s' of a deleted Game Object (use '.Exists' to check before accessing a member)", key);

	ReflectorRef ref;

	if (const Reflection::PropertyDescriptor* prop = obj->FindProperty(key, &ref))
	{
		Reflection::GenericValue gv = prop->Get(ref.Referred());

#ifndef NDEBUG
		Reflection::ValueType base = prop->Type;
		if ((uint8_t)base > (uint8_t)Reflection::ValueType::Null)
			base = (Reflection::ValueType)((uint8_t)base - (uint8_t)Reflection::ValueType::Null);

		assert(gv.Type == base
			|| ((uint8_t)prop->Type > (uint8_t)Reflection::ValueType::Null && gv.Type == Reflection::ValueType::Null)
		);
#endif

		ScriptEngine::L::PushGenericValue(L, gv);
	}

	else if (const Reflection::EventDescriptor* event = obj->FindEvent(key, &ref))
		pushSignal(L, event, ref, key);

	// Methods are lower because we prefer namecalls
	else if (const Reflection::MethodDescriptor* func = obj->FindMethod(key, &ref))
		ScriptEngine::L::PushMethod(L, func, ref);

	else
	{
		GameObject* child = obj->FindChild(key);

		if (child)
			ScriptEngine::L::PushGameObject(L, child);
		else
			// 18/05/2025
			// this is going to be an error because i spent an entire 26 seconds
			// trying to figure out why something wasnt working
			luaL_error(L, "No child or member '%s' of %s", key, obj->GetFullName().c_str());
	}

	return 1;
};

static int api_gameobjnewindex(lua_State* L)
{
	ZoneScopedNC("GameObject.__newindex", tracy::Color::LightSkyBlue);

	GameObject* obj = GameObject::FromGenericValue(ScriptEngine::L::ToGeneric(L, 1));
	const char* key = luaL_checkstring(L, 2);

	ZoneText(key, strlen(key));

	if (strcmp(key, "Exists") == 0)
		luaL_error(L, "%s", "'Exists' is read-only! - 21/12/2024");

	LUA_ASSERT(obj, "Tried to assign to the '%s' of a deleted Game Object", key);

	if (const Reflection::PropertyDescriptor* prop = obj->FindProperty(key))
	{
		if (!prop->Set)
		{
			const char* argTypeName = luaL_typename(L, 3);
			const char* argAsString = luaL_tolstring(L, 3, nullptr);

			luaL_error(L, 
				"Cannot set '%s' to '%s' (%s) because it is read-only",
				key, argAsString, argTypeName
			);
		}

		ScriptEngine::L::CheckType(L, prop->Type, 3);
		Reflection::GenericValue newValue = ScriptEngine::L::ToGeneric(L, 3);

		try
		{
			obj->SetPropertyValue(key, newValue);
		}
		catch (const std::runtime_error& err)
		{
			luaL_error(L, "Error while setting property '%s' of %s: %s", key, obj->GetFullName().c_str(), err.what());
		}
	}
	else
	{
		std::string fullname = obj->GetFullName();

		if (obj->FindChild(key))
			luaL_error(L,
				"Attempt to set invalid Member '%s' of '%s', although it has a child object with that name",
				key, fullname.c_str()
			);
		else
			luaL_error(L,
				"Attempt to set invalid Member '%s' of '%s'",
				key, fullname.c_str()
			);
	}

	return 0;
};

static int api_gameobjectnamecall(lua_State* L)
{
	ZoneScopedNC("GameObject.__namecall", tracy::Color::LightSkyBlue);

	GameObject* g = GameObject::FromGenericValue(ScriptEngine::L::ToGeneric(L, 1));
	const char* k = L->namecall->data; // this is weird 10/01/2025

	if (!g)
		luaL_error(L, "Tried to call '%s' of a de-allocated GameObject with ID %u", k, *(uint32_t*)lua_touserdata(L, 1));

	ZoneText(k, strlen(k));

	ReflectorRef reflector;
	const Reflection::MethodDescriptor* func = g->FindMethod(k, &reflector);

	if (!func)
		luaL_error(L, "'%s' is not a valid method of %s", k, g->GetFullName().c_str());

	int numresults = 0;

	try
	{
		numresults = ScriptEngine::L::HandleMethodCall(
			L,
			func,
			reflector
		);
	}
	catch (const std::runtime_error& err)
	{
		luaL_error(L, "Error while invoking method '%s' of %s: %s", k, g->GetFullName().c_str(), err.what());
	}

	return numresults;
}

static int api_gameobjecttostring(lua_State* L)
{
	GameObject* object = GameObject::FromGenericValue(ScriptEngine::L::ToGeneric(L, 1));
	
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
	size_t ksize = 0;
	const char* key = luaL_checklstring(L, 2, &ksize);

	lua_getglobal(L, "Color");
	lua_pushstring(L, key);
	lua_rawget(L, -2);

	// Pass-through to Vector3.new
	if (!lua_isnil(L, -1))
		return 1;

	if (ksize == 1)
	{
		switch (key[0])
		{
		case 'R':
			lua_pushnumber(L, vec->R);
			break;
		case 'G':
			lua_pushnumber(L, vec->G);
			break;
		case 'B':
			lua_pushnumber(L, vec->B);
			break;
		default:
			luaL_error(L, "attempt to index Color with '%s'", key);
		}

		return 1;
	}

	luaL_error(L, "attempt to index Color with '%s'", key);
};

static int api_coltostring(lua_State* L)
{
	Color* col = (Color*)luaL_checkudata(L, 1, "Color");

	lua_pushfstringL(L, "%f, %f, %f", col->R, col->G, col->B);
	return 1;
};

static int api_eventnamecall(lua_State* L)
{
	if (strcmp(L->namecall->data, "Connect") == 0)
	{
		luaL_checktype(L, 2, LUA_TFUNCTION);

		lua_Debug callerAr{};
		lua_getinfo(L, 1, "sln", &callerAr);
		std::string callerInfo = std::format(
			"{}:{} in {}",
			callerAr.short_src,
			callerAr.currentline,
			callerAr.name ? callerAr.name : "<anonymous>"
		);

		EventSignalData* ev = (EventSignalData*)luaL_checkudata(L, 1, "EventSignal");
		const Reflection::EventDescriptor* rev = ev->Event;
		int signalRef = lua_ref(L, 1);

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
		luaL_sandboxthread(cL);

		EventConnectionData* ec = (EventConnectionData*)lua_newuserdata(eL, sizeof(EventConnectionData));
		ec->Script.Reference = nullptr; // zero-initialization breaks some assumptions that IDs which are not `PHX_GAMEOBJECT_NULL_ID` are valid
		luaL_getmetatable(eL, "EventConnection"); // stack: ec, mt
		lua_setmetatable(eL, -2); // stack: ec
		lua_xpush(eL, L, -1);

		// assign the connection to the Event Thread so it can be used by the Connection Threads
		// `eL` itself is never resumed, only `cL` or `nL` (`nL` being created per-invocation if the thread yields every time)
		lua_pushlightuserdata(eL, eL); // stack: ec, lud
		lua_pushvalue(eL, -2); // stack: ec, lud, ec
		lua_settable(eL, LUA_ENVIRONINDEX); // stack: ec
		lua_pop(eL, 1);

		uint32_t cnId = rev->Connect(
			ev->Reflector.Referred(),
		
			[eL, cL, rev, callerInfo](const std::vector<Reflection::GenericValue>& Inputs)
			-> void
			{
				ZoneScopedN("RunEventCallback");
				ZoneText(callerInfo.data(), callerInfo.size());

				assert(Inputs.size() == rev->CallbackInputs.size());
				assert(lua_isfunction(eL, 2));

				lua_pushlightuserdata(eL, eL);
				lua_gettable(eL, LUA_ENVIRONINDEX);
				EventConnectionData* cn = (EventConnectionData*)luaL_checkudata(eL, -1, "EventConnection");
				lua_pop(eL, 1);

				if (cn->Script.HasValue())
				{
					GameObject* scr = (GameObject*)cn->Script.Dereference();

					if (!scr || !scr->FindComponentByType(EntityComponent::Script))
					{
						cn->Event->Disconnect(cn->Reflector.Referred(), cn->ConnectionId);
						lua_resetthread(cL);
						lua_resetthread(eL);

						return;
					}
				}
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

					luaL_sandboxthread(nL);
					co = nL;
				}
				else
					lua_xpush(eL, cL, 2);

				for (size_t i = 0; i < Inputs.size(); i++)
				{
					assert(Inputs[i].Type == rev->CallbackInputs[i]);
					ScriptEngine::L::PushGenericValue(co, Inputs[i]);
				}

				ZoneNamedN(pcallzone, "pcall", true);

				// TODO 11/08/2025
				// they added a "correct" way to do this, with continuations n stuff
				// but i genuinely just could not be bothered
				co->baseCcalls++;
				int status = lua_pcall(co, static_cast<int32_t>(Inputs.size()), 0, 0);
				co->baseCcalls--;
				// TODO 06/09/2025
				// i dont even know
				// is this intentional??
				// only a problem when the status is YIELD or BREAK,
				// in which case `lua_pcall` misleading returns `0`
				// other times it doesnt matter
				status = status == 0 ? co->status : status;

				if (status != LUA_OK && status != LUA_YIELD && status != LUA_BREAK)
				{
					luaL_checkstack(co, 2, "error string"); // tolstring may push a metatable temporarily as well
					Log::ErrorF("Script event: {}", luaL_tolstring(co, -1, nullptr));
					ScriptEngine::L::DumpStacktrace(co);
					lua_pop(co, 2);
				}

				if (status == LUA_BREAK)
				{
					cn->CallbackYields = true;
					while (co->status == LUA_BREAK)
						lua_resume(co, nullptr, 0); // i dont knowwwwww
				}

				if (!cn->CallbackYields && status == LUA_YIELD)
					cn->CallbackYields = true;

				if (status == LUA_OK && cn->CallbackYields && cL->status == LUA_OK)
				{ // unmark the callback as yielding if we didnt yield and the original thread is ready to be re-used
					lua_pop(cL, lua_gettop(cL)); // not entirely sure how these are piling up
					cn->CallbackYields = false;
				}
			}
		);

		ec->Reflector = ev->Reflector;
		ec->SignalRef = signalRef;
		ec->ThreadRef = threadRef;
		ec->ConnectionId = cnId;
		ec->Event = rev;
		ec->L = L;

		lua_getglobal(L, "script");
		Reflection::GenericValue scrgv = ScriptEngine::L::ToGeneric(L, -1);
		if (scrgv.Type == Reflection::ValueType::GameObject)
		{
			GameObject* scr = GameObject::FromGenericValue(scrgv);
			ec->Script = scr;
		}

		lua_pop(L, 1);
	
		return 1;
	}
	else if (strcmp(L->namecall->data, "WaitUntil") == 0)
	{
		EventSignalData* ev = (EventSignalData*)luaL_checkudata(L, 1, "EventSignal");
		const Reflection::EventDescriptor* rev = ev->Event;

		ReflectorRef reflector = ev->Reflector;
		bool* resume = new bool;
		std::vector<Reflection::GenericValue>* values = new std::vector<Reflection::GenericValue>;
		*resume = false;

		uint32_t cid = rev->Connect(
			reflector.Referred(),

			[ev, resume, reflector, rev, values](const std::vector<Reflection::GenericValue>& Values)
			-> void
			{
				*values = Values;
				*resume = true;
			}
		);

		ScriptEngine::L::Yield(
			L,
			rev->CallbackInputs.size(),
			[resume, values, reflector, rev, cid](ScriptEngine::YieldedCoroutine& yc)
			-> void
			{
				yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Polled;
				yc.RmPoll = [resume, values, reflector, rev, cid](lua_State* L) -> int
					{
						if (*resume)
						{
							rev->Disconnect(reflector.Referred(), cid);

							int count = (int)values->size();

							for (const Reflection::GenericValue& gv : *values)
								ScriptEngine::L::PushGenericValue(L, gv);

							delete values;
							delete resume;

							return count;
						}

						return -1;
					};
			}
		);

		return -1;
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
	{
		lua_pushinteger(L, ec->SignalRef);
		lua_gettable(L, LUA_REGISTRYINDEX);
	}
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

		ec->Event->Disconnect(ec->Reflector.Referred(), ec->ConnectionId);
		ec->ConnectionId = UINT32_MAX;

		// errors with `attempt to modify readonly table`
		//lua_pushlightuserdata(L, L);
		//lua_pushnil(L);
		//lua_settable(L, LUA_ENVIRONINDEX); // remove stacktrace string

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
			*outSize = strpath.size() + 1;

			if (bufferSize < strpath.size() + 1)
				return WRITE_BUFFER_TOO_SMALL;
			else
			{
				memcpy(buffer + 1, strpath.data(), strpath.size());
				buffer[0] = '@';
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

			bool success = true;
			std::string contents = FileRW::ReadFile(((*curpath) / ".luaurc").string(), &success);
			*outSize = contents.size();

			if (bufferSize < contents.size())
				return WRITE_BUFFER_TOO_SMALL;
			
			memcpy(buffer, contents.data(), contents.size());

			return success ? WRITE_SUCCESS : WRITE_FAILURE;
		};
	config->load = [](lua_State* L, void* ctx, const char* /* path */, const char* chname, const char* ldname)
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

			bool readSuccess = true;
			std::string contents = FileRW::ReadFile(curpath->string(), &readSuccess);

			if (!readSuccess)
			{
				lua_pop(GL, 1);
				luaL_error(L, "%s", contents.c_str());
				return 1;
			}

			if (ScriptEngine::CompileAndLoad(ML, contents, chname) == 0)
			{
				lua_pushstring(ML, ldname);
				lua_setglobal(ML, "_LOADNAME");

				int status = lua_resume(ML, L, 0);

				if (status == LUA_OK)
				{
					if (lua_gettop(ML) == 0)
						lua_pushstring(ML, "module must return a value");
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

	assert(!config->get_alias);
}

// FROM: `ldblib.cpp` 15/08/2025
static lua_State* getthread(lua_State* L, int* arg)
{
    if (lua_isthread(L, 1))
    {
        *arg = 1;
        return lua_tothread(L, 1);
    }
    else
    {
        *arg = 0;
        return L;
    }
}

lua_State* ScriptEngine::L::Create(const std::string& VmName)
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	lua_State* state = lua_newstate(l_alloc, nullptr);
	// Load Standard Library ('print' etc)
	luaL_openlibs(state);
	// Load runtime-specific libraries
	luhx_openlibs(state);

	std::filesystem::path* requirePath = new std::filesystem::path;
	luaopen_require(
		state,
		requireConfigInit,
		requirePath
	);
	
	lua_getglobal(state, "_G");
	lua_pushinteger(state, 67);
	lua_pushlightuserdatatagged(state, requirePath, 67);
	lua_settable(state, -3);
	lua_pop(state, 1);

	lua_getglobal(state, "debug");
	lua_pushcfunction(
		state,
		[](lua_State* L)
		{
			int arg;
    		lua_State* L1 = getthread(L, &arg);
    		const char* msg = luaL_optstring(L, arg + 1, NULL);
    		int level = luaL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    		luaL_argcheck(L, level >= 0, arg + 2, "level can't be negative");

			std::string trace;
			ScriptEngine::L::DumpStacktrace(L, &trace, level, msg);

			lua_pushlstring(L, trace.data(), trace.size());
			return 1;
		},
		"hx_dumpStacktrace"
	);
	lua_setfield(state, -2, "traceback");

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
					luaL_error(
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
				size_t klen = 0;
				const char* k = luaL_checklstring(L, 2, &klen);

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

				else if (klen == 4 && k[0] == 'C' && k[2] == 'R')
				{
					char col = k[1];
					char row = k[3];

					// allows ASCII 49, 50, 51, 52, AKA
					// 1, 2, 3, and 4
					luaL_argcheck(L, col > 48 && col < 53, 2, "column index must be in range [1 .. 4]");
					luaL_argcheck(L, row > 48 && row < 53, 2, "row index must be in range [1 .. 4]");

					lua_pushnumber(L, m[col - 49][row - 49]);
				}
				else
					luaL_error(L, "Invalid member %s", k);

				return 1;
			},
			"Matrix.__index"
		);
		lua_setfield(state, -2, "__index");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				ZoneScopedNC("Matrix.__newindex", tracy::Color::LightSkyBlue);

				glm::mat4& m = *(glm::mat4*)luaL_checkudata(L, 1, "Matrix");
				size_t klen = 0;
				const char* k = luaL_checklstring(L, 2, &klen);
				float v = static_cast<float>(luaL_checknumber(L, 3));

				ZoneText(k, strlen(k));

				if (klen == 4 && k[0] == 'C' && k[2] == 'R')
				{
					char col = k[1];
					char row = k[3];

					// allows ASCII 49, 50, 51, 52, AKA
					// 1, 2, 3, and 4
					luaL_argcheck(L, col > 48 && col < 53, 2, "column index must be in range [1 .. 4]");
					luaL_argcheck(L, row > 48 && row < 53, 2, "row index must be in range [1 .. 4]");

					// push previous value
					lua_pushnumber(L, m[col - 49][row - 49]);
					m[col - 49][row - 49] = v;
				}
				else
					luaL_error(L, "Invalid member %s", k);

				return 1;
			},
			"Matrix.__newindex"
		);
		lua_setfield(state, -2, "__newindex");

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

		{
			lua_createtable(state, (int)EntityComponent::__count, 0);

			for (uint8_t i = 1; i < (int)EntityComponent::__count; i++)
			{
				lua_pushinteger(state, i);
				lua_pushlstring(state, s_EntityComponentNames[i].data(), s_EntityComponentNames[i].length());
				lua_settable(state, -3);
			}

			lua_setfield(state, -2, "validComponents");
		}

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

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				EventSignalData* ev1 = (EventSignalData*)luaL_checkudata(L, 1, "EventSignal");
				EventSignalData* ev2 = (EventSignalData*)luaL_checkudata(L, 2, "EventSignal");

				lua_pushboolean(L, ev1->Event == ev2->Event && ev1->Reflector == ev2->Reflector);
				return 1;
			},
			"EventSignal.__eq"
		);
		lua_setfield(state, -2, "EventSignal.__eq");

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				EventSignalData* ev = (EventSignalData*)luaL_checkudata(L, 1, "EventSignal");
				GameObject* obj = GameObject::GetObjectById(ev->Reflector.Id);

				std::string source = ev->Reflector.Type == EntityComponent::None
					? (obj ? obj->GetFullName() + "." : "GameObject::")
					: std::format("{}::", s_EntityComponentNames[(size_t)ev->Reflector.Type]);

				lua_pushfstring(L, "%s%s", source.c_str(), ev->EventName);
				return 1;
			},
			"EventSignal.__tostring"
		);
		lua_setfield(state, -2, "__tostring");

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

		lua_pushcfunction(
			state,
			[](lua_State* L)
			{
				EventConnectionData* ec = (EventConnectionData*)luaL_checkudata(L, 1, "EventConnection");
				lua_pushinteger(L, ec->SignalRef);
				lua_gettable(L, LUA_REGISTRYINDEX);

				EventSignalData* ev = (EventSignalData*)luaL_checkudata(L, -1, "EventSignal");
				GameObject* obj = GameObject::GetObjectById(ev->Reflector.Id);

				std::string source = ev->Reflector.Type == EntityComponent::None
					? (obj ? obj->GetFullName() + "." : "GameObject::")
					: std::format("{}::", s_EntityComponentNames[(size_t)ev->Reflector.Type]);

				lua_pushfstring(L, "Connection to %s%s", source.c_str(), ev->EventName);
				return 1;
			},
			"EventConnection.__tostring"
		);
		lua_setfield(state, -2, "__tostring");

		lua_pushstring(state, "EventConnection");
		lua_setfield(state, -2, "__type");
	}

	ScriptEngine::L::PushGameObject(state, GameObject::GetObjectById(GameObject::s_DataModel));
	lua_setglobal(state, "game");

	ScriptEngine::L::PushGameObject(state, GameObject::GetObjectById(GameObject::s_DataModel)->FindChild("Workspace"));
	lua_setglobal(state, "workspace");

	lua_pushlstring(state, VmName.data(), VmName.size());
	lua_setglobal(state, "_VMNAME");

	if (L::DebugBreak)
	{
		state->global->cb.debugbreak = [](lua_State* L, lua_Debug* ar)
			{
				Log::Info("Debug breakpoint");
				L::DebugBreak(L, ar, false, false);
			};
		state->global->cb.debuginterrupt = [](lua_State* L, lua_Debug* ar)
			{
				assert(false); // TODO should this be triggering?
				Log::Info("Debug interrupt - MAYBE SHOULDN'T BE OCCURRING");
				L::DebugBreak(L, ar, false, true);
			};

		state->global->cb.debugprotectederror = [](lua_State* L)
			{
				lua_Debug ar;
				lua_getinfo(L, 1, "sln", &ar);
				Log::Info("Debug protected error");
				L::DebugBreak(L, &ar, true, false);
			};
	}

	luaL_sandbox(state);
	return state;
}

void ScriptEngine::L::Close(lua_State* L)
{
	lua_getglobal(L, "_G");
	lua_pushinteger(L, 67);
	lua_gettable(L, -2);
	delete (std::filesystem::path*)lua_tolightuserdatatagged(L, -1, 67);
	
	lua_close(L);
}

nlohmann::json ScriptEngine::DumpApiToJson()
{
	ObjectRef tempdm = GameObject::Create("DataModel");
	ObjectRef tempwp = GameObject::Create("Workspace");
	tempwp->SetParent(tempdm);
	GameObject::s_DataModel = tempdm->ObjectId;
	
	lua_State* base = lua_newstate(l_alloc, nullptr);
	lua_State* luhx = L::Create("ApiDump");
	// Load Standard Library ('print' etc)
	luaL_openlibs(base);
	lua_pushinteger(base, 0);
	lua_setglobal(base, "require");

	// Compare the environment of our extended environment ("Luhx")
	// with the standard Luau environment to figure out what got added
	nlohmann::json json;

	lua_getglobal(luhx, "_G");
	lua_pushnil(luhx);
	while (lua_next(luhx, -2))
	{
		if (lua_islightuserdata(luhx, -1))
		{
			// 67
			lua_pop(luhx, 1);
			continue;
		}

		std::string k = luaL_checkstring(luhx, -2);

		lua_getglobal(base, k.c_str());
		if (lua_isnil(base, -1))
		{
			if (!lua_istable(luhx, -1))
			{
				const char* tn = luaL_typename(luhx, -1);

				if (strcmp(tn, "GameObject") == 0)
				{
					std::string type = tn;

					GameObject* obj = GameObject::FromGenericValue(L::ToGeneric(luhx, -1));

					for (const ReflectorRef& ref : obj->Components)
					{
						type.append(" & ");
						type.append(s_EntityComponentNames[(uint8_t)ref.Type]);
					}

					json["Globals"][k] = type;
				}
				else
					json["Globals"][k] = tn;
			}
			else
			{
				nlohmann::json lib;

				lua_pushnil(luhx);
				while (lua_next(luhx, -2))
				{
					lib[luaL_checkstring(luhx, -2)] = luaL_typename(luhx, -1);

					lua_pop(luhx, 1);
				}

				if (luaL_getmetatable(luhx, k.c_str()) == LUA_TTABLE)
				{
					json["Datatypes"][k]["Library"] = lib;

					lua_pushnil(luhx);
					while (lua_next(luhx, -2))
					{
						json["Datatypes"][k]["Metatable"][luaL_checkstring(luhx, -2)] = luaL_typename(luhx, -1);
						lua_pop(luhx, 1);
					}
				}
				else
					json["Libraries"][k] = lib;

				lua_pop(luhx, 1);
			}
		}

		lua_pop(base, 1);
		lua_pop(luhx, 1);
	}

	nlohmann::json& eventSignal = json["Datatypes"]["EventSignal"];
	eventSignal = nlohmann::json::object();
	nlohmann::json& eventConnection = json["Datatypes"]["EventConnection"];
	eventConnection = nlohmann::json::object();

	json["Globals"]["script"] = "GameObject & Script";

	lua_getglobal(luhx, "_G");
	lua_pushinteger(luhx, 67);
	lua_gettable(luhx, -2);
	delete (std::filesystem::path*)lua_tolightuserdatatagged(luhx, -1, 67);
	lua_pop(luhx, 1);

	lua_close(base);
	lua_close(luhx);

	GameObject::s_DataModel = PHX_GAMEOBJECT_NULL_ID;
	tempdm->Destroy();
	
	return json;
}
