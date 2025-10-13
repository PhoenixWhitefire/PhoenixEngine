#include <filesystem>

#include "script/luhx.hpp"
#include "FileRW.hpp"

static std::string unifyPath(const std::string& Str)
{
	std::string newStr = Str;

	for (char& c : newStr)
		if (c == '\\')
			c = '/';

	return newStr;
}

static int fs_write(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
	const char* contents = luaL_checkstring(L, 2);
	std::string errorMessage;

    bool success = FileRW::WriteFileCreateDirectories(path, contents, &errorMessage);

	if (success)
	{
    	lua_pushboolean(L, true);
		return 1;
	}
	else
	{
		lua_pushboolean(L, false);
		lua_pushlstring(L, errorMessage.data(), errorMessage.size());
		return 2;
	}
}

static int fs_read(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);

    bool success = true;
    std::string contents = FileRW::ReadFile(path, &success);

	if (success)
		lua_pushlstring(L, contents.data(), contents.size());
	else
		lua_pushnil(L);
	
	return 1;
}

static int fs_listdir(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    size_t filterstrlen = 0;
	const char* filterstr = luaL_optlstring(L, 2, "a", &filterstrlen);

    luaL_argcheck(L, filterstrlen == 1, 2, "must be exactly 1 character long");
    char filter = filterstr[0];
    luaL_argcheck(L, filter == 'a' || filter == 'f' || filter == 'd', 2, "expected 'a', 'f', or 'd'");

	lua_newtable(L);

	for (const auto& entry : std::filesystem::directory_iterator(FileRW::MakePathCwdRelative(path)))
	{
		switch (filter)
		{
		case 'f':
		{
			if (std::filesystem::is_regular_file(entry))
			{
				std::string entryPath = unifyPath(entry.path().string());
				lua_pushstring(L, entryPath.c_str());
				lua_pushstring(L, "f");
				lua_settable(L, -3);
			}
			break;
		}
		case 'd':
		{
			if (std::filesystem::is_directory(entry))
			{
				std::string entryPath = unifyPath(entry.path().string());
				lua_pushstring(L, entryPath.c_str());
				lua_pushstring(L, "d");
				lua_settable(L, -3);
			}
			break;
		}
		default:
		{
			std::string entryPath = unifyPath(entry.path().string());
			lua_pushstring(L, entryPath.c_str());

			if (std::filesystem::is_directory(entry))
				lua_pushstring(L, "d");
			else
				lua_pushstring(L, "f");

			lua_settable(L, -3);
			break;
		}
		}
	}

	return 1;
}

static int fs_isfile(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
	lua_pushboolean(L, std::filesystem::is_regular_file(FileRW::MakePathCwdRelative(path)));

	return 1;
}

static int fs_isdirectory(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
	lua_pushboolean(L, std::filesystem::is_directory(FileRW::MakePathCwdRelative(path)));

	return 1;
}

#include <SDL3/SDL_dialog.h>

#include "script/ScriptEngine.hpp"

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

static int fs_ispromptactive(lua_State* L)
{
    lua_pushboolean(L, IsFdInProgress);
    return 1;
}

static int fs_promptsave(lua_State* L)
{
    if (IsFdInProgress)
		luaL_errorL(L, "The User has not completed the previous File Dialog");
	
	std::string defaultLocation = FileRW::MakePathAbsolute(luaL_optstring(L, 1, "./"));
	SDL_DialogFileFilter filter{};
	filter.pattern = luaL_optstring(L, 2, "*");
	filter.name = luaL_optstring(L, 3, "All files");

	ScriptEngine::L::Yield(
		L,
		1,
		[filter, defaultLocation](ScriptEngine::YieldedCoroutine& yc)
		{
			IsFdInProgress = true;
	
			SDL_ShowSaveFileDialog(
				fdCallback,
				NULL,
				SDL_GL_GetCurrentWindow(),
				&filter,
				1,
				defaultLocation.c_str()
			);

			yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Polled;
			yc.RmPoll = [](lua_State* CL)
				-> int
				{
					if (IsFdInProgress)
						return -1;
					else
					{
						if (FdResults.size() == 0)
							lua_pushnil(CL);
						else
							lua_pushstring(CL, FileRW::MakePathCwdRelative(FdResults[0]).c_str());
					
						return 1;
					}
				};
		}
	);

	return -1;
}

static int fs_promptopen(lua_State* L)
{
    if (IsFdInProgress)
		luaL_errorL(L, "The User has not completed the previous File Dialog");

	std::string defaultLocation = FileRW::MakePathAbsolute(luaL_optstring(L, 1, "./"));
	SDL_DialogFileFilter filter{};
	filter.pattern = luaL_optstring(L, 2, "*");
	filter.name = luaL_optstring(L, 3, "All files");

	bool allowMultipleFiles = luaL_optboolean(L, 4, false);

	ScriptEngine::L::Yield(
		L,
		1,
		[&](ScriptEngine::YieldedCoroutine& yc)
		{
			IsFdInProgress = true;
		
			SDL_ShowOpenFileDialog(
				fdCallback,
				NULL,
				SDL_GL_GetCurrentWindow(),
				&filter,
				1,
				defaultLocation.c_str(),
				allowMultipleFiles
			);

			yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Polled;
			yc.RmPoll = [](lua_State* CL)
				-> int
				{
					if (IsFdInProgress)
						return -1;
					else
					{
						lua_newtable(CL);
					
						for (int i = 0; i < (int)FdResults.size(); i++)
						{
							lua_pushinteger(CL, i + 1);
							lua_pushstring(CL, FileRW::MakePathCwdRelative(FdResults[i]).c_str());
							lua_settable(CL, -3);
						}
					
						return 1;
					}
				};
		}
	);

	return -1;
}

static const luaL_Reg fs_funcs[] =
{
    { "write", fs_write },
    { "read", fs_read },
    { "listdir", fs_listdir },
    { "isfile", fs_isfile },
    { "isdirectory", fs_isdirectory },

	{ "ispromptactive", fs_ispromptactive },
    { "promptsave", fs_promptsave },
    { "promptopen", fs_promptopen },
    { NULL, NULL }
};

int luhxopen_fs(lua_State* L)
{
    luaL_register(L, LUHX_FSLIBNAME, fs_funcs);

    return 1;
}
