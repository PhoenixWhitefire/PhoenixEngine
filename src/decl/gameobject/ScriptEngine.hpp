// 21/09/2024

#pragma once

#include<unordered_map>
#include<future>
#include<thread>
#include<luau/Compiler/include/luacode.h>
#include<luau/VM/include/lualib.h>
#include<luau/Common/include/Luau/Common.h>
#include<glm/mat4x4.hpp>

#include"datatype/Vector3.hpp"
#include"datatype/GameObject.hpp"
#include"UserInput.hpp"
#include"Reflection.hpp"
#include"FileRW.hpp"

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")

#include<WinSock2.h>
#include<WS2tcpip.h>
#endif

static std::vector<uint64_t> s_ConnectedSockets;

template <class T> void throwWrapped(T exc)
{
	throw(exc);
}

// Calls both `shutdown` and `closesocket` on the provider socket descriptor
// `errCallback` is a function that intakes a string and returns void, eg
// `Debug::Log` or `throwWrapped`
static void closeNetSocket(size_t sockindex, std::function<void(std::string)> errCallback)
{
	uint64_t sock = s_ConnectedSockets.at(sockindex);

	s_ConnectedSockets.erase(s_ConnectedSockets.begin() + sockindex);

	int shutdownStatus = shutdown(sock, SD_BOTH);
	if (shutdownStatus == SOCKET_ERROR)
	{
		int errCode = WSAGetLastError();
		errCallback(std::vformat(
			"Failed to `shutdown` on socket {}, error code: {}",
			std::make_format_args(sock, errCode)
		));
	}

	int closeStatus = closesocket(sock);
	if (closeStatus != SOCKET_ERROR)
	{
		int errCode = WSAGetLastError();
		errCallback(std::vformat(
			"Failed to `closesocket` on socket {}, error code: {}",
			std::make_format_args(sock, errCode)
		));
	}
}

static void net_init()
{
#ifdef _WIN32
	static bool s_DidInit = false;

	struct AppShutdownNotifier
	{
		~AppShutdownNotifier()
		{
			if (s_DidInit)
			{
				Debug::Log("Shutting down WSA");

				size_t numSocks = s_ConnectedSockets.size();

				if (numSocks > 0)
				{
					Debug::Log(std::vformat(
						"Closing {} sockets...",
						std::make_format_args(numSocks)
					));

					for (size_t index = 0; index < s_ConnectedSockets.size(); index++)
						closeNetSocket(index, Debug::Log);
				}

				WSACleanup();
			}
		}
	};

	// call `WSACleanup` on shutdown
	// (specically static object destruction point)
	static AppShutdownNotifier shutdownNotifier;

	if (s_DidInit)
		return;

	WSADATA data{};
	int result = WSAStartup(MAKEWORD(2, 2), &data);

	if (result != 0)
		throw(std::vformat(
			"`WSAStartup` failed: {}",
			std::make_format_args(result)
		));

	s_DidInit = true;
#else
	throw("Networking is only available on Windows distributions of Phoenix Engine - 21/09/2024")
#endif
}

// "Scheduled Coroutines"
// The `lua_State*` should be `lua_resume`'d upon the `std::shared_future` becoming valid
// 22/09/2024
// I think this is possible?
static std::unordered_map<lua_State*, std::shared_future<Reflection::GenericValue>> s_ScheduledCoroutines{};

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
extern Reflection::GenericValue luaTypeToGeneric(lua_State* L, int luaT, int stackIndex = -1)
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

extern void pushVector3(lua_State* L, const Vector3& vec)
{
	void* ptrTovec = lua_newuserdata(L, sizeof(Vector3));
	*(Vector3*)ptrTovec = vec;

	luaL_getmetatable(L, "Vector3");
	lua_setmetatable(L, -2);
}

extern void pushMatrix(lua_State* L, const glm::tmat4x4<float>& Matrix)
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

extern void pushGenericValue(lua_State* L, const Reflection::GenericValue& gv)
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
extern void pushFunction(lua_State* L, const char* name)
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

extern std::unordered_map<std::string, lua_CFunction> GlobalScriptFunctions =
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

		pushGenericValue(L, mtx);

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
		bool locked = luaL_checkboolean(L, 1);

		if (locked)
			SDL_SetWindowMouseGrab(SDL_GetGrabbedWindow(), SDL_TRUE);
		else
			SDL_SetWindowMouseGrab(SDL_GetGrabbedWindow(), SDL_FALSE);

		return 0;
	}
	},

	{
		"sleep",
		[](lua_State* L)
		{
			int sleepTime = luaL_checkinteger(L, 1);

			auto a = std::async(
				std::launch::async,
				[](int st)
				{
					std::this_thread::sleep_for(std::chrono::seconds(st));
					return Reflection::GenericValue(st);
				},
				sleepTime
			);
			s_ScheduledCoroutines.insert(std::pair(L, a.share()));

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
	}
	},

	{
		"net_host",
		[](lua_State* L)
		{
			net_init();

			struct addrinfo* addrInfo = NULL, hints;

			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;
			hints.ai_flags = AI_PASSIVE;
			
			// Resolve the local address and port to be used by the server
			int addrInfoResult = getaddrinfo(NULL, luaL_checkstring(L, 1), &hints, &addrInfo);
			if (addrInfoResult != 0)
				throw(std::vformat(
					"`getaddrinfo` failed for `net_host`, error code: {}",
					std::make_format_args(addrInfoResult)
				));

			SOCKET listenSock = INVALID_SOCKET;
			listenSock = socket(addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol);

			if (listenSock == INVALID_SOCKET)
			{
				int errCode = WSAGetLastError();
				throw(std::vformat(
					"`socket` failed in `net_host`, error code: {}",
					std::make_format_args(errCode)
				));
			}

			s_ConnectedSockets.push_back(listenSock);

			int bindingResult = bind(listenSock, addrInfo->ai_addr, (int)addrInfo->ai_addrlen);
			if (bindingResult == SOCKET_ERROR)
			{
				int errCode = WSAGetLastError();
				
				throw(std::vformat(
					"`bind` failed in `net_host`, error code: {}",
					std::make_format_args(errCode)
				));
			}

			int makeListener = listen(listenSock, SOMAXCONN);
			if (makeListener == SOCKET_ERROR)
			{
				int errCode = WSAGetLastError();
				throw(std::vformat(
					"`listen` failed in `net_host`, error code: {}",
					std::make_format_args(errCode)
				));
			}

			freeaddrinfo(addrInfo);

			lua_pushinteger(L, static_cast<int>(listenSock));
			
			return 1;
		}
	},

	{
		"net_accept",
		[](lua_State* L)
		{
			net_init();

			SOCKET listenSock = luaL_checkinteger(L, 1);

			auto a = std::async(
				std::launch::async,
				[](SOCKET lsock)
				{
					SOCKET acceptSocket = accept(lsock, NULL, NULL);
					if (acceptSocket == INVALID_SOCKET)
					{
						int errCode = WSAGetLastError();
						throw(std::vformat(
							"`accept` failed in `net_accept`, error code: {}",
							std::make_format_args(errCode)
						));
					}

					return Reflection::GenericValue(static_cast<uint32_t>(acceptSocket));
				},
				listenSock
			);
			s_ScheduledCoroutines.insert(std::pair(L, a.share()));

			return lua_yield(L, 1);
		}
	},

	{
		"net_connect",
		[](lua_State* L)
		{
			net_init();

			const char* targetname = luaL_checkstring(L, 1);
			const char* targetport = luaL_checkstring(L, 2);

			// https://learn.microsoft.com/en-us/windows/win32/winsock/creating-a-socket-for-the-client
					// 21/09/2024
			struct addrinfo* addrInfo = NULL,
				* ptr = NULL,
				hints;

			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;

			int getInfoResult = getaddrinfo(targetname, targetport, &hints, &addrInfo);
			if (getInfoResult != 0)
				throw(std::vformat(
					"`getaddrinfo` failed in `net_connect`, error code: {}",
					std::make_format_args(getInfoResult)
				));

			SOCKET connectSocket = INVALID_SOCKET;

			ptr = addrInfo;

			connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
			if (connectSocket == INVALID_SOCKET)
			{
				int errCode = WSAGetLastError();
				throw(std::vformat(
					"`socket` failed in `net_connect`, error code: {}",
					std::make_format_args(errCode)
				));
			}

			s_ConnectedSockets.push_back(connectSocket);

			int connectResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (connectResult == SOCKET_ERROR)
				throw("Unable to connect to server!");

			uint32_t truncatedSocketId = static_cast<uint32_t>(connectSocket);

			lua_pushinteger(L, truncatedSocketId);

			return 1;
		}
	},

	{
		"net_send",
		[](lua_State* L)
		{
			net_init();

			SOCKET sock = luaL_checkinteger(L, 1);
			const char* data = luaL_checkstring(L, 2);
			
			auto a = std::async(
				std::launch::async,
				[](SOCKET s, std::string d)
				{
					int sendResult = send(s, d.c_str(), static_cast<int>(d.size()), 0);
					if (sendResult == SOCKET_ERROR)
					{
						int errCode = WSAGetLastError();
						throw(std::vformat(
							"`send` failed in `net_send`. Please ensure you are using the correct socket. Error code: {}",
							std::make_format_args(errCode)
						));
					}

					return Reflection::GenericValue(sendResult);
				},
				sock,
				std::string(data) // 22/09/2024 Copy the data...? idrk if it's necessary TODO
			);
			s_ScheduledCoroutines.insert(std::pair(L, a.share()));

			return lua_yield(L, 1);
		}
	},
	
	{
		"net_receive",
		[](lua_State* L)
		{
			net_init();

			SOCKET sock = luaL_checkinteger(L, 1);
			int recvBufCapacity = luaL_checkinteger(L, 2);

			if (recvBufCapacity <= 0)
				throw("Receive buffer size (argument #2) must be > 0");

			char* recvBuf = (char*)malloc(recvBufCapacity);

			if (recvBuf == NULL)
				throw(std::vformat(
					"Could not allocate a buffer of {} bytes",
					std::make_format_args(recvBufCapacity)
				));

			auto a = std::async(
				std::launch::async,
				[](SOCKET s, char* buf, int bufCap)
				{
					int recvResult = recv(s, buf, bufCap, 0);

					std::vector<Reflection::GenericValue> returnVal;
					returnVal.push_back(recvResult);

					if (recvResult >= 0)
						returnVal.push_back(buf);

					return Reflection::GenericValue(recvResult);
				},
				sock,
				recvBuf,
				recvBufCapacity
			);
			s_ScheduledCoroutines.insert(std::pair(L, a.share()));

			return lua_yield(L, 1);
		}
	},

	{
		"net_close",
		[](lua_State* L)
		{
			net_init();

			int sock = luaL_checkinteger(L, 1);

			closeNetSocket(sock, throwWrapped<std::string>);

			return 0;
		}
	}
};
