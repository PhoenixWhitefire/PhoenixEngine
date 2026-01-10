#include <luau/Require/include/Luau/Require.h>
#include <luau/VM/include/lualib.h>
#include <luau/VM/src/lstate.h>
#include <Luau/Compiler.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

#include <tracy/Tracy.hpp>

#include "script/ScriptEngine.hpp"
#include "script/luhx.hpp"
#include "script/enum/keys.hpp"
#include "datatype/Color.hpp"
#include "GlobalJsonConfig.hpp"
#include "Engine.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

// depends on the ordering of `Reflection::ValueType`!!
static const lua_Type s_ValueTypeToLuauType[] = {
	LUA_TNIL,

	LUA_TBOOLEAN,
	LUA_TNUMBER, // Integer
	LUA_TNUMBER, // Double
	LUA_TSTRING,

	LUA_TUSERDATA, // Color
	LUA_TVECTOR,   // Vector2
	LUA_TVECTOR,   // Vector3
	LUA_TUSERDATA, // Matrix
	LUA_TUSERDATA, // GameObject

	LUA_TFUNCTION,

	LUA_TTABLE, // Array
	LUA_TTABLE, // Map
};
static_assert(std::size(s_ValueTypeToLuauType) == Reflection::ValueType::__lastBase);

#define ROOT_LVM_NAME "RootLVM"

static int luauAssertHandler(const char* expression, const char* file, int line, const char* function)
{
	RAISE_RTF("Luau assertion failed:\n\tExpression: {}\n\tIn: {}:{} in {}", expression, file, line, function);
}

void ScriptEngine::Initialize()
{
	RegisterNewVM(ROOT_LVM_NAME);
	CurrentVM = ROOT_LVM_NAME;

	// changing a reference to a static function variable
	Luau::assertHandler() = luauAssertHandler;
}

const ScriptEngine::LuauVM& ScriptEngine::RegisterNewVM(const std::string& Name)
{
	const auto& it = VMs.find(Name);
	if (it != VMs.end())
		RAISE_RT("A VM already exists with that name");

	VMs[Name] = {
		.Name = Name,
		.MainThread = L::Create(Name)
	};

	return VMs[Name];
}

const ScriptEngine::LuauVM& ScriptEngine::GetCurrentVM()
{
	assert(VMs.find(CurrentVM) != VMs.end());
	return VMs[CurrentVM];
}

lua_Type ScriptEngine::ReflectionTypeToLuauType(Reflection::ValueType rvt)
{
	assert(size_t(rvt & ~Reflection::ValueType::Null) < std::size(s_ValueTypeToLuauType));
	return s_ValueTypeToLuauType[rvt & ~Reflection::ValueType::Null];
}

static int shouldResume_Wait(
	const ScriptEngine::YieldedCoroutine& CorInfo,
	lua_State* L
)
{
	if (double curTime = GetRunningTime(); curTime >= CorInfo.RmWait.ResumeAt)
	{
		lua_pushnumber(L, curTime - CorInfo.RmWait.YieldedAt);
		return 1;
	}
	else
		return -1;
}

static int shouldResume_Deferred(
	const ScriptEngine::YieldedCoroutine& CorInfo,
	lua_State* L
)
{
	if (double curTime = GetRunningTime(); curTime >= CorInfo.RmWait.ResumeAt)
	{
		int narg = lua_gettop(CorInfo.RmDeferred.Arguments);
		lua_xmove(CorInfo.RmDeferred.Arguments, L, narg);
		lua_unref(L, CorInfo.RmDeferred.ArgumentsRef);

		return narg;
	}
	else
		return -1;
}

static int shouldResume_Future(const ScriptEngine::YieldedCoroutine& CorInfo, lua_State* L)
{
	const std::shared_future<std::vector<Reflection::GenericValue>>& future = CorInfo.RmFuture;

	if ( future.valid()
		 && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready
	)
	{
		std::vector<Reflection::GenericValue> returnVals = future.get();
		for (const Reflection::GenericValue& v : returnVals)
			ScriptEngine::L::PushGenericValue(L, v);

		return (int)returnVals.size();
	}
	else
		return -1;
}

static int shouldResume_Polled(const ScriptEngine::YieldedCoroutine& CorInfo, lua_State* L)
{
	return CorInfo.RmPoll(L);
}

typedef int(*ResumptionModeHandler)(const ScriptEngine::YieldedCoroutine&, lua_State*);

static const ResumptionModeHandler s_ResumptionModeHandlers[] =
{
	nullptr,

	shouldResume_Wait,
	shouldResume_Deferred,
	shouldResume_Future,
	shouldResume_Polled
};

void ScriptEngine::StepScheduler()
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	size_t size = s_YieldedCoroutines.size();

	for (size_t i = 0; i < size; i++)
	{
		YieldedCoroutine* it = &s_YieldedCoroutines[i];

		// make sure the script still exists
		// modules don't have a `script` global, but they run on their own coroutine independent from
		// where they are `require`'d from anyway
		if (it->ScriptId != PHX_GAMEOBJECT_NULL_ID)
		{
			GameObject* scr = GameObject::GetObjectById(it->ScriptId);
			if (!scr || scr->IsDestructionPending || !scr->FindComponentByType(EntityComponent::Script))
			{
				s_YieldedCoroutines.erase(s_YieldedCoroutines.begin() + i);

				// https://stackoverflow.com/a/17956637
				// asan was not happy about the iterator from
				// `::erase` for some reason?? TODO
				// 10/06/2025
				if (size != s_YieldedCoroutines.size())
				{
					i--;
					size = s_YieldedCoroutines.size();
				}

				continue;
			}
		}

		lua_State* coroutine = it->Coroutine;
		int corRef = it->CoroutineReference;

		const ResumptionModeHandler handler = s_ResumptionModeHandlers[it->Mode];
		assert(handler);

		int nretvals = handler(*it, coroutine);

		if (nretvals >= 0)
		{
			ZoneScopedN("Resume");
			ZoneText(it->DebugString.data(), it->DebugString.size());

			int resumeStatus = L::Resume(coroutine, nullptr, nretvals);
			if (resumeStatus != LUA_OK && resumeStatus != LUA_YIELD)
			{
				Log::ErrorF(
					"Script resumption: {}",
					lua_tostring(coroutine, -1)
				);

				L::DumpStacktrace(coroutine);
			}

			lua_unref(coroutine, corRef);

			s_YieldedCoroutines.erase(s_YieldedCoroutines.begin() + i);
		}

		// https://stackoverflow.com/a/17956637
		// asan was not happy about the iterator from
		// `::erase` for some reason?? TODO
		// 10/06/2025
		if (size != s_YieldedCoroutines.size())
		{
			i--;
			size = s_YieldedCoroutines.size();
		}
	}
}

#define JSON_ENCODED_DATA_TAG "__HX_EncodedData"

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
			std::string key = it.key();
			const nlohmann::json& data = it.value();

			if (key == JSON_ENCODED_DATA_TAG)
			{
				const std::string& type = data["type"];
				const nlohmann::json& encoded = data["data"];

				if (type == "vector")
				{
					lua_pop(L, 1);
					lua_pushvector(L, encoded[0], encoded[1], encoded[2]);
					return;
				}
				else
					luaL_error(L, "Unknown encoded datatype '%s'", type.c_str());
			}
			else
				PushJson(L, data);

			lua_setfield(L, -2, key.c_str());
		}

		break;
	}
	default:
	{
		assert(false);
		lua_pushfstring(L, "< JSON Value : %s >", v.type_name());
	}
	}
}

#define ERROR_CONTEXTUALIZED_NVARARGS(e) { \
if (Context.size() > 0) \
	luaL_error(L, e " (serializing %s)", Context.c_str()); \
else \
	luaL_error(L, e); } \

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

				if (strcmp(key, JSON_ENCODED_DATA_TAG) == 0)
					ERROR_CONTEXTUALIZED_NVARARGS("The table key '" JSON_ENCODED_DATA_TAG "' is reserved");

				if (Context.size() == 0)
					Context = "Dictionary";

				t[key] = L::ToJson(L, -1, Context + "." + key);
			}

			lua_pop(L, 1);
		}
		lua_pop(L, 1);

		return t;
	}

	case LUA_TVECTOR:
	{
		const float* vec = luaL_checkvector(L, StackIndex);

		nlohmann::json value;
		nlohmann::json& data = value[JSON_ENCODED_DATA_TAG];
		data["type"] = "vector";
		data["data"][0] = vec[0];
		data["data"][1] = vec[1];
		data["data"][2] = vec[2];

		return value;
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

	lua_setreadonly(L, LUA_GLOBALSINDEX, false);
	lua_pushlstring(L, ChunkName.data(), ChunkName.size());
	lua_setglobal(L, "_CHUNKNAME");

	int result = luau_load(L, ChunkName.c_str(), bytecode.data(), bytecode.size(), 0);
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

					lua_setfield(CL, LUA_ENVIRONINDEX, YIELDBLOCKERTRACKING); // leaves same empty table at stack top after popping B
				}

				int ybsize = lua_objlen(CL, -1);
				lua_pushinteger(CL, ybsize + 1);
				lua_pushstring(CL, fndbinfo.c_str());
				lua_settable(CL, -3);
				lua_pop(CL, 1);

				int status = L::ProtectedCall(CL, (int)Inputs.size(), -1, 0);

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

	bool isOptional = Type & Reflection::ValueType::Null;
	int givenType = lua_type(L, StackIndex);

	if (!isOptional || givenType != LUA_TNIL)
	{
		lua_Type lty = ReflectionTypeToLuauType(Type);

		// the literal `if` check inside this function likes to take 190 microseconds sometimes in Debug mode for some reason
		// probably some cache bullshit
		// fuck
		luaL_checktype(L, StackIndex, lty);

		if (lty == LUA_TUSERDATA)
		{
			// dont want the type name to end with `?`
			// it needs to match with the metatable's `__type`
			Reflection::ValueType baseTy = Reflection::ValueType(Type & ~Reflection::ValueType::Null);
			luaL_checkudata(L, StackIndex, Reflection::TypeAsString(baseTy).c_str());
		}
		else if (lty == LUA_TVECTOR && (Type & ~Reflection::ValueType::Null) == Reflection::ValueType::Vector2)
		{
			luaL_argcheck(L, luaL_checkvector(L, StackIndex)[2] == 0.f, StackIndex, "Z component must be 0");
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
	case Reflection::ValueType::Vector2:
	{
		luhx_pushvector3(L, glm::vec3(gv.AsVector2(), 0.f));
		break;
	}
	case Reflection::ValueType::Vector3:
	{
		luhx_pushvector3(L, gv.AsVector3());
		break;
	}
	case Reflection::ValueType::Color:
	{
		luhx_pushcolor(L, gv);
		break;
	}
	case Reflection::ValueType::Matrix:
	{
		luhx_pushmatrix(L, gv.AsMatrix());
		break;
	}
	case Reflection::ValueType::GameObject:
	{
		luhx_pushgameobject(L, GameObject::FromGenericValue(gv));
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

		lua_createtable(L, 0, (uint32_t)array.size() / 2);

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

		if (!(param & Reflection::ValueType::Null))
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
	else if (numArgs > minArgs)
	{
		int clip = numArgs;

		for (int arg = numArgs; arg > 0; arg--)
		{
			if (lua_isnil(L, arg + 1))
				clip = arg - 1;
			else
				break;
		}

		numArgs = clip;
	}
	else if (numArgs > numParams)
	{
		lua_Debug ar = {};
		lua_getinfo(L, 1, "sln", &ar);

		// this is just so that the Output can attribute the log message to a script
		std::string extraTags = std::format("ScriptFunctionName:{},TextDocument:{},DocumentLine:{}", ar.name ? ar.name : "<anonymous>", ar.short_src , ar.currentline);

		Log::Warning(
			std::format("Function received {} more arguments than necessary", numArgs - numParams),
			extraTags
		);
	}

	assert(luaL_checkudata(L, 1, "GameObject"));
	std::vector<Reflection::GenericValue> inputs;

	for (int index = 0; index < numArgs; index++)
	{
		Reflection::ValueType paramType = paramTypes[index];

		// offset the stack so the error message gives the correct argument number
		// without this, the first argument would be reported as "argument #2"
		L->base++;
		ScriptEngine::L::CheckType(L, paramType, index + 1);
		L->base--;
		inputs.push_back(L::ToGeneric(L, index + 2));

		if (paramType == Reflection::ValueType::Vector2)
			inputs.back().Type = Reflection::ValueType::Vector2; // no native Vector2 type, but `vector` still works fine
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
		assert(Reflection::TypeFits(func->Outputs[i], output.Type));

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

int ScriptEngine::L::Yield(lua_State* L, int NumResults, std::function<void(YieldedCoroutine&)> Configure)
{
	ZoneScoped;

	if (ImGuiContext* ctx = ImGui::GetCurrentContext(); ctx && ctx->CurrentWindowStack.Size > 1)
	{
		lua_Debug ar;
		lua_getinfo(L, 0, "n", &ar);
		RAISE_RTF(
			"Cannot yield with '{}' while in Dear ImGui section",
			ar.name ? ar.name : "<unknown>"
		);	
	}

	lua_Debug ar = {};
	lua_getinfo(L, 1, "sln", &ar);

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

		lua_Debug yieldar;
		lua_getinfo(L, 0, "n", &yieldar);

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
		lua_Debug yieldar;
		lua_getinfo(L, 0, "n", &yieldar);
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

	s_YieldedCoroutines.push_back(YieldedCoroutine{
		.Coroutine = L,
		.CoroutineReference = lua_ref(L, -1),
		.ScriptId = scriptId,
		.Mode = YieldedCoroutine::ResumptionMode::INVALID
	});
	YieldedCoroutine& yc = s_YieldedCoroutines.back();

	std::string traceback;
	L::DumpStacktrace(L, &traceback);

	yc.DebugString = traceback;

	Configure(yc);
	assert(yc.Mode != YieldedCoroutine::ResumptionMode::INVALID);

	return -1; // like lua_yield
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

	if (const std::string* deferredTrace = (const std::string*)L->userdata)
	{
		if (Into)
		{
			Into->append("-- Deferred from --\n");
			Into->append(*deferredTrace);
		}
		else
		{
			Log::AppendF("-- Deferred from--\n{}", *deferredTrace);
		}
	}
}

static void* l_alloc(void*, void* ptr, size_t, size_t nsize)
{
	assert(nsize <= UINT32_MAX);

	if (nsize == 0)
	{
		Memory::Free(ptr);
		return NULL;
	}
	else
		return Memory::ReAlloc(ptr, (uint32_t)nsize, Memory::Category::Luau);
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
	config->get_config_status = [](lua_State*, void* ctx)
		{
			bool hasConfigScript = std::filesystem::is_regular_file(*(std::filesystem::path*)ctx / ".config.luau");
			bool hasLuauRc = std::filesystem::is_regular_file(*(std::filesystem::path*)ctx / ".luaurc");
			
			if (hasConfigScript && hasLuauRc)
				return CONFIG_AMBIGUOUS;
			if (!hasConfigScript && !hasLuauRc)
				return CONFIG_ABSENT;
			
			return hasConfigScript ? CONFIG_PRESENT_LUAU : CONFIG_PRESENT_JSON;
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

				int status = ScriptEngine::L::Resume(ML, L, 0);

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

static void openEnum(lua_State* L)
{
	lua_newtable(L);

	lua_newtable(L);

	for (int i = GLFW_KEY_SPACE; i <  GLFW_KEY_LAST; i++)
	{
		const char* kn = KeyNames[i];
		if (!kn)
			continue;

		lua_pushinteger(L, i);
		lua_setfield(L, -2, kn);
	}

	lua_setfield(L, -2, "Key");

	lua_newtable(L);

	lua_pushinteger(L, GLFW_MOUSE_BUTTON_LEFT);
	lua_setfield(L, -2, "Left");

	lua_pushinteger(L, GLFW_MOUSE_BUTTON_MIDDLE);
	lua_setfield(L, -2, "Middle");

	lua_pushinteger(L, GLFW_MOUSE_BUTTON_RIGHT);
	lua_setfield(L, -2, "Right");

	lua_setfield(L, -2, "MouseButton");

	lua_newtable(L);

	lua_pushinteger(L, GLFW_CURSOR_CAPTURED);
	lua_setfield(L, -2, "Captured");

	lua_pushinteger(L, GLFW_CURSOR_DISABLED);
	lua_setfield(L, -2, "Disabled");

	lua_pushinteger(L, GLFW_CURSOR_HIDDEN);
	lua_setfield(L, -2, "Hidden");

	lua_pushinteger(L, GLFW_CURSOR_NORMAL);
	lua_setfield(L, -2, "Normal");

	lua_pushinteger(L, GLFW_CURSOR_UNAVAILABLE);
	lua_setfield(L, -2, "Unavailable");

	lua_setfield(L, -2, "CursorMode");

	lua_newtable(L);

	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "None");

	lua_pushinteger(L, 1);
	lua_setfield(L, -2, "Back");

	lua_pushinteger(L, 2);
	lua_setfield(L, -2, "Front");

	lua_setfield(L, -2, "FaceCulling");

	lua_newtable(L);

	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "Aabb");

	lua_pushinteger(L, 1);
	lua_setfield(L, -2, "AabbStaticSize");

	lua_setfield(L, -2, "CollisionFidelity");

	lua_newtable(L);

	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "None");

	lua_pushinteger(L, 1);
	lua_setfield(L, -2, "Info");

	lua_pushinteger(L, 2);
	lua_setfield(L, -2, "Warning");

	lua_pushinteger(L, 3);
	lua_setfield(L, -2, "Error");

	lua_setfield(L, -2, "LogType");

	lua_setglobal(L, "Enum");
}

lua_State* ScriptEngine::L::Create(const std::string& VmName)
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	lua_State* state = lua_newstate(l_alloc, nullptr);
	// Load Standard Library ('print' etc)
	luaL_openlibs(state);
	// Load runtime-specific libraries
	luhx_openlibs(state);

	openEnum(state);

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
	lua_pop(state, 1);

	luhx_pushgameobject(state, GameObject::GetObjectById(GameObject::s_DataModel));
	lua_setglobal(state, "game");

	luhx_pushgameobject(state, GameObject::GetObjectById(GameObject::s_DataModel)->FindChild("Workspace"));
	lua_setglobal(state, "workspace");

	lua_pushlstring(state, VmName.data(), VmName.size());
	lua_setglobal(state, "_VMNAME");

	if (L::DebugBreak)
	{
		state->global->cb.debugbreak = [](lua_State* L, lua_Debug* ar)
			{
				L::DebugBreak(L, ar, DebugBreakReason::Breakpoint);
			};
		state->global->cb.debuginterrupt = [](lua_State* L, lua_Debug* ar)
			{
				L::DebugBreak(L, ar, DebugBreakReason::Interrupt);
			};
	}

	state->global->cb.userthread = [](lua_State* LP, lua_State* L)
		{
			if (!LP)
			{
				if (L->userdata)
				{
					delete (std::string*)L->userdata; // assigned in `defer` for debugger :)
					L->userdata = nullptr;
				}
			}
		};

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

	size_t size = s_YieldedCoroutines.size();

	for (size_t i = 0; i < s_YieldedCoroutines.size(); i++)
	{
		YieldedCoroutine& yc = s_YieldedCoroutines[i];
		if (lua_mainthread(yc.Coroutine) == L)
			s_YieldedCoroutines.erase(s_YieldedCoroutines.begin() + i);

		// https://stackoverflow.com/a/17956637
		// asan was not happy about the iterator from
		// `::erase` for some reason?? TODO
		// 10/06/2025
		if (size != s_YieldedCoroutines.size())
		{
			i--;
			size = s_YieldedCoroutines.size();
		}
	}
}

static void breakHere(lua_State* L, ScriptEngine::L::DebugBreakReason Reason)
{
	using namespace ScriptEngine;

	if (L::DebugBreak)
	{
		lua_Debug ar = {};
		lua_getinfo(L, 1, "sln", &ar);
		L::DebugBreak(L, &ar, Reason);
	}
}

static lua_Status finishCoroutine(lua_State* L, int status)
{
	using namespace ScriptEngine;
	using namespace L;

	while (status == LUA_BREAK)
	{
		breakHere(L, DebugBreakReason::BrokeIntoDebugger);
		status = lua_resume(L, nullptr, 0);
	}

	if (status == LUA_ERRRUN)
		breakHere(L, DebugBreakReason::Error);

	if (LeaveDebugger)
		LeaveDebugger();

	return (lua_Status)status;
}

lua_Status ScriptEngine::L::Resume(lua_State* L, lua_State* from, int narg)
{
	int status = lua_resume(L, from, narg);
	return finishCoroutine(L, status);
}

lua_Status ScriptEngine::L::ProtectedCall(lua_State* L, int narg, int nret, int errfunc)
{
	int status = lua_pcall(L, narg, nret, errfunc);
	return finishCoroutine(L, status);
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
