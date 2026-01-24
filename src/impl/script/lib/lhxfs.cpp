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
	bool createDirectories = luaL_optboolean(L, 3, false);
	std::string errorMessage;

    bool success = createDirectories ?
					FileRW::WriteFileCreateDirectories(path, contents, &errorMessage)
					: FileRW::WriteFile(path, contents, &errorMessage);


	if (!success)
		luaL_error(L, "%s", errorMessage.c_str());

	return 0;
}

static int fs_read(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);

    bool success = true;
    std::string contents = FileRW::ReadFile(path, &success);

	if (success)
	{
		lua_pushlstring(L, contents.data(), contents.size());
		return 1;
	}
	else
	{
		lua_pushnil(L);
		lua_pushlstring(L, contents.data(), contents.size());
		return 2;
	}
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

static int fs_definealias(lua_State* L)
{
	const char* aliasName = luaL_checkstring(L, 1);
	if (aliasName[0] == '@')
		luaL_error(L, "invalid alias name '%s' - alias names should not begin with '@' as it is most likely a misunderstanding", aliasName);

	FileRW::DefineAlias(aliasName, luaL_checkstring(L, 2));
	return 0;
}

static int fs_makecwdaliasof(lua_State* L)
{
	FileRW::MakeCwdAliasOf(luaL_checkstring(L, 1));
	return 1;
}

static int fs_cwd(lua_State* L)
{
	std::string cwd = std::filesystem::current_path().string();
	lua_pushlstring(L, cwd.data(), cwd.size());
	return 1;
}

static int fs_copy(lua_State* L)
{
	std::error_code ec;

	std::string from = FileRW::MakePathCwdRelative(luaL_checkstring(L, 1));
	std::string to = FileRW::MakePathCwdRelative(luaL_checkstring(L, 2));
	std::filesystem::copy(
		from,
		to,
		std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
		ec
	);

	if (ec)
		luaL_error(L, "'%s' -> '%s': %s", from.c_str(), to.c_str(), ec.message().c_str());

	return 0;
}

static int fs_mkdir(lua_State* L)
{
	std::error_code ec;
	std::string realPath = FileRW::MakePathCwdRelative(luaL_checkstring(L, 1));
	std::filesystem::create_directory(realPath, ec);

	if (ec)
		luaL_error(L, "'%s': %s", realPath.c_str(), ec.message().c_str());

	return 0;
}

static int fs_rename(lua_State* L)
{
	std::error_code ec;

	std::string path = FileRW::MakePathCwdRelative(luaL_checkstring(L, 1));
	std::string name = luaL_checkstring(L, 2);

	std::string fullnewpath = path.substr(0, path.find_last_of('/') + 1) + name;
	std::filesystem::rename(path, fullnewpath, ec);

	if (ec)
		luaL_error(L, "'%s' ren '%s': %s", path.c_str(), fullnewpath.c_str(), ec.message().c_str());

	return 0;
}

static int fs_remove(lua_State* L)
{
	std::error_code ec;
	int numRemoved = (int)std::filesystem::remove_all(FileRW::MakePathCwdRelative(luaL_checkstring(L, 1)), ec);

	if (ec)
		luaL_error(L, "%s", ec.message().c_str());
	else
	{
		lua_pushinteger(L, numRemoved);
		return 1;
	}
}

#include <tinyfiledialogs.h>

#include "script/ScriptEngine.hpp"

static std::string normalizePath(std::string path)
{
	for (size_t i = 0; i < path.size(); i++)
		if (path[i] == '\\')
			path[i] = '/';

	return FileRW::MakePathCwdRelative(path);
}

static int fs_promptsave(lua_State* L)
{
	std::string defaultLocation = FileRW::MakePathAbsolute(luaL_optstring(L, 1, "./"));
	std::vector<const char*> filters;

	if (lua_isstring(L, 2))
	{
		filters.push_back(lua_tostring(L, 2));
	}
	else
	{
		lua_pushnil(L);
		while (lua_next(L, 2))
		{
			filters.push_back(luaL_checkstring(L, -1));
			lua_pop(L, 1);
		}
	}
	
	const char* filterName = luaL_optstring(L, 3, nullptr);

	const char* path = tinyfd_saveFileDialog(
		"Save File",
		!lua_isnoneornil(L, 1) ? defaultLocation.c_str() : nullptr,
		(int)filters.size(),
		filters.data(),
		filterName
	);

	if (path)
		lua_pushstring(L, normalizePath(path).c_str());
	else
		lua_pushnil(L);

	return 1;
}

static int fs_promptopen(lua_State* L)
{
	std::string defaultLocation = FileRW::MakePathAbsolute(luaL_optstring(L, 1, "./"));
	std::vector<const char*> filters;

	if (lua_isstring(L, 2))
	{
		filters.push_back(lua_tostring(L, 2));
	}
	else
	{
		lua_pushnil(L);
		while (lua_next(L, 2))
		{
			filters.push_back(luaL_checkstring(L, -1));
			lua_pop(L, 1);
		}
	}

	const char* filterName = luaL_optstring(L, 3, nullptr);
	bool allowMultipleFiles = luaL_optboolean(L, 4, false);

	const char* filescstr = tinyfd_openFileDialog(
		"Open File(s)",
		!lua_isnoneornil(L, 1) ? defaultLocation.c_str() : nullptr,
		(int)filters.size(),
		filters.data(),
		filterName,
		allowMultipleFiles
	);

	lua_newtable(L);

	if (filescstr)
	{
		size_t lastOff = 0;
		std::string filesstr = filescstr;
		std::string file = "";
		int nfiles = 0;

		for (size_t i = 0; i < filesstr.size(); i++)
		{
			if (filesstr[i] == '|')
			{
				nfiles++;

				lua_pushinteger(L, nfiles);
				lua_pushstring(L, normalizePath(file).c_str());
				lua_settable(L, -3);

				lastOff = i+1;
				file.clear();
				continue;
			}

			file = std::string(filesstr.begin() + lastOff, filesstr.begin() + i + 1);
		}

		if (file.size() != 0)
		{
			lua_pushinteger(L, 1);
			lua_pushstring(L, normalizePath(file).c_str());
			lua_settable(L, -3);
		}
	}

	return 1;
}

static int fs_promptopenfolder(lua_State* L)
{
	const char* path = tinyfd_selectFolderDialog(
		luaL_checkstring(L, 1),
		luaL_optstring(L, 2, nullptr)
	);

	if (path)
		lua_pushstring(L, path);
	else
		lua_pushnil(L);

	return 1;
}

static const luaL_Reg fs_funcs[] =
{
    { "write", fs_write },
    { "read", fs_read },
    { "listdir", fs_listdir },
    { "isfile", fs_isfile },
    { "isdirectory", fs_isdirectory },
	{ "definealias", fs_definealias },
	{ "makecwdaliasof", fs_makecwdaliasof },
	{ "cwd", fs_cwd },
	{ "copy", fs_copy },
	{ "mkdir", fs_mkdir },
	{ "rename", fs_rename },
	{ "remove", fs_remove },

    { "promptsave", fs_promptsave },
    { "promptopen", fs_promptopen },
	{ "promptopenfolder", fs_promptopenfolder },
    { NULL, NULL }
};

int luhxopen_fs(lua_State* L)
{
    luaL_register(L, LUHX_FSLIBNAME, fs_funcs);

    return 1;
}
