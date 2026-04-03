#include <filesystem>
#include <chrono>

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wtemplate-id-cdtor"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wunused-result"
#endif

#include <Vendor/filewatch/FileWatch.hpp>

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

#include "script/luhx.hpp"
#include "FileRW.hpp"

// windows??
#ifdef Yield
#undef Yield
#endif

static std::string unifyPath(const std::string& Str)
{
	std::string newStr = Str;

	for (char& c : newStr)
		if (c == '\\')
			c = '/';

	return newStr;
}

static void setSelfAlias(lua_State* L)
{
	lua_Debug ar = {};
	if (!lua_getinfo(L, 1, "s", &ar) || strlen(ar.short_src) < 2)
	{
		FileRW::DefineAlias("self", "");
		FileRW::DefineAlias("selfdir", "");
		return;
	}

	FileRW::DefineAlias("self", ar.short_src);
	FileRW::DefineAlias("selfdir", std::filesystem::path(ar.short_src).parent_path().string());
}

static int fs_write(lua_State* L)
{
	setSelfAlias(L);
	size_t len = 0;

    const char* path = luaL_checkstring(L, 1);
	const char* contents = luaL_checklstring(L, 2, &len);
	bool createDirectories = luaL_optboolean(L, 3, false);
	std::string errorMessage;

    bool success = createDirectories ?
					FileRW::WriteFileCreateDirectories(path, std::string_view(contents, len), &errorMessage)
					: FileRW::WriteFile(path, std::string_view(contents, len), &errorMessage);

	if (!success)
		luaL_error(L, "%s", errorMessage.c_str());

	return 0;
}

static int fs_read(lua_State* L)
{
	setSelfAlias(L);

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
	setSelfAlias(L);

    const char* path = luaL_checkstring(L, 1);
    size_t filterstrlen = 0;
	const char* filterstr = luaL_optlstring(L, 2, "a", &filterstrlen);

    luaL_argcheck(L, filterstrlen == 1, 2, "must be exactly 1 character long");
    char filter = filterstr[0];
    luaL_argcheck(L, filter == 'a' || filter == 'f' || filter == 'd', 2, "expected 'a', 'f', or 'd'");

	std::error_code ec;
	lua_newtable(L);

	for (const auto& entry : std::filesystem::directory_iterator(FileRW::ResolvePathNormalized(path), ec))
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

	if (ec)
		luaL_error(L, "listdir '%s': %s", FileRW::ResolvePathNormalized(path).c_str(), ec.message().c_str());

	return 1;
}

static int fs_isfile(lua_State* L)
{
	setSelfAlias(L);

    const char* path = luaL_checkstring(L, 1);
	lua_pushboolean(L, std::filesystem::is_regular_file(FileRW::ResolvePathNormalized(path)));

	return 1;
}

static int fs_isdirectory(lua_State* L)
{
	setSelfAlias(L);

    const char* path = luaL_checkstring(L, 1);
	lua_pushboolean(L, std::filesystem::is_directory(FileRW::ResolvePathNormalized(path)));

	return 1;
}

static int fs_definealias(lua_State* L)
{
	const char* aliasName = luaL_checkstring(L, 1);
	if (aliasName[0] == '@')
		luaL_error(L, "invalid alias name '%s' - alias names should not begin with '@' as it is prepended automatically", aliasName);

	FileRW::DefineAlias(aliasName, luaL_checkstring(L, 2));
	return 0;
}

static int fs_removealias(lua_State* L)
{
	const char* aliasName = luaL_checkstring(L, 1);
	if (aliasName[0] == '@')
		luaL_error(L, "invalid alias name '%s' - alias names should not begin with '@' as it is prepended automatically", aliasName);

	FileRW::RemoveAlias(aliasName);
	return 0;
}

static int fs_setunqualifiedroot(lua_State* L)
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
	setSelfAlias(L);

	std::error_code ec;

	std::string from = FileRW::ResolvePathNormalized(luaL_checkstring(L, 1));
	std::string to = FileRW::ResolvePathNormalized(luaL_checkstring(L, 2));
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
	setSelfAlias(L);

	std::error_code ec;
	std::string realPath = FileRW::ResolvePathNormalized(luaL_checkstring(L, 1));
	std::filesystem::create_directory(realPath, ec);

	if (ec)
		luaL_error(L, "'%s': %s", realPath.c_str(), ec.message().c_str());

	return 0;
}

static int fs_rename(lua_State* L)
{
	setSelfAlias(L);

	std::error_code ec;

	std::string path = FileRW::ResolvePathNormalized(luaL_checkstring(L, 1));
	std::string name = luaL_checkstring(L, 2);

	if (name.find('/') != std::string::npos)
		luaL_error(L, "'%s' rename N:'%s': New name cannot have slashes (use `fs.move`)", path.c_str(), name.c_str());

	std::string fullnewpath = path.substr(0, path.find_last_of('/') + 1) + name;
	std::filesystem::rename(path, fullnewpath, ec);

	if (ec)
		luaL_error(L, "'%s' rename P:'%s': %s", path.c_str(), fullnewpath.c_str(), ec.message().c_str());

	return 0;
}

static int fs_move(lua_State* L)
{
	setSelfAlias(L);

	std::error_code ec;

	std::string path = FileRW::ResolvePathNormalized(luaL_checkstring(L, 1));
	std::string newPath = luaL_checkstring(L, 2);

	std::filesystem::rename(path, newPath, ec);

	if (ec)
		luaL_error(L, "'%s' move '%s': %s", path.c_str(), newPath.c_str(), ec.message().c_str());

	return 0;
}

static int fs_remove(lua_State* L)
{
	setSelfAlias(L);

	std::error_code ec;
	int numRemoved = (int)std::filesystem::remove_all(FileRW::ResolvePathNormalized(luaL_checkstring(L, 1)), ec);

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

	return FileRW::ResolvePathNormalized(path);
}

static int fs_promptsave(lua_State* L)
{
	setSelfAlias(L);

	std::string defaultLocation = FileRW::ResolvePathAbsolute(luaL_optstring(L, 1, "./"));
	std::vector<const char*> filters;

	if (lua_isstring(L, 2))
	{
		filters.push_back(lua_tostring(L, 2));
	}
	else if (lua_istable(L, 2))
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
	setSelfAlias(L);

	std::string defaultLocation = FileRW::ResolvePathAbsolute(luaL_optstring(L, 1, "./"));
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
	setSelfAlias(L);

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

// 13/01/2025: https://stackoverflow.com/a/478960
// 02/12/2025: copied from Main.cpp
#ifdef _WIN32

#define popen _popen
#define pclose _pclose

#endif
static std::string exec(const char* cmd)
{
	std::array<char, 128> buffer{ 0 };
	std::string result;
	FILE* pipe = popen(cmd, "r");

	if (!pipe)
		throw std::runtime_error("popen() failed!");

	while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
		result += buffer.data();

	pclose(pipe);

	return result;
}

static int fs_execute(lua_State* L)
{
	const char* command = luaL_checkstring(L, 1);

	return ScriptEngine::L::Yield(
		L,
		1,
		[command](ScriptEngine::YieldedCoroutine& yc)
		{
			std::shared_future<std::string> f = std::async(std::launch::async, [command]()
			{
				std::string result = exec(command);
				return result;
			}).share();

			yc.RmPoll = [f](lua_State* L) -> int
			{
				if (!f.valid() || (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready))
					return -1;

				const std::string& ret = f.get();
				lua_pushlstring(L, ret.data(), ret.size());

				return 1;
			};
			yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Polled;
		}
	);
}

static int fs_resolvepath(lua_State* L)
{
	setSelfAlias(L);

	const std::string& resolved = FileRW::ResolvePathNormalized(luaL_checkstring(L, 1));

	lua_pushlstring(L, resolved.data(), resolved.size());
	return 1;
}

static int fs_resolvepathabsolute(lua_State* L)
{
	setSelfAlias(L);

	const std::string& resolved = FileRW::ResolvePathAbsolute(luaL_checkstring(L, 1));

	lua_pushlstring(L, resolved.data(), resolved.size());
	return 1;
}

struct WatcherData
{
	filewatch::FileWatch<std::string>* Watcher = nullptr;
	lua_State* WL = nullptr;
	int Ref = LUA_NOREF;
};

static void disconnectWatcher(WatcherData* data)
{
	delete data->Watcher;
	lua_unref(data->WL, data->Ref);
	lua_resetthread(data->WL);

	delete data;
}

static int disconnectWatcherLFunc(lua_State* L)
{
	if (lua_isnil(L, lua_upvalueindex(1)))
		luaL_error(L, "Disconnect function called multiple times");

	void* data = lua_tolightuserdata(L, lua_upvalueindex(1));
	disconnectWatcher((WatcherData*)data);

	lua_pushnil(L);
	lua_replace(L, lua_upvalueindex(1));

	return 0;
}

static int fs_watch(lua_State* L)
{
	luaL_checktype(L, 2, LUA_TFUNCTION);

	std::string path = luaL_checkstring(L, 1);

	lua_State* WL = lua_newthread(L);
	int ref = lua_ref(L, -1);
	lua_pop(L, 1);
	lua_xpush(L, WL, 2);

	WatcherData* data = new WatcherData{
		.WL = WL,
		.Ref = ref
	};

	filewatch::FileWatch<std::string>* watcher = new filewatch::FileWatch<std::string>(
		path,
		[WL, path, data](const std::string& File, filewatch::Event Type)
		{
			std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(ScriptEngine::s_ParallelFileWatcherEventsMutex);

			ScriptEngine::s_ParallelFileWatcherEvents.push_back(ScriptEngine::FileWatcherEvent{
				.Thread = WL,
				.File = File,
				.Type = (int)Type
			});
		}
	);

	data->Watcher = watcher;

	lua_pushlightuserdata(L, data);
	lua_pushcclosure(
		L,
		disconnectWatcherLFunc,
		"disconnectFileWatcher",
		1
	);

	return 1;
}

static int fs_lastwritten(lua_State* L)
{
	std::string path = FileRW::ResolvePathNormalized(luaL_checkstring(L, 1));
	std::filesystem::file_time_type lwt = std::filesystem::last_write_time(path);

	auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(lwt);
	size_t secondsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(systemTime.time_since_epoch()).count();

	lua_pushnumber(L, (double)secondsSinceEpoch);
	return 1;
}

static const luaL_Reg fs_funcs[] = {
    { "write", fs_write },
    { "read", fs_read },
    { "listdir", fs_listdir },
    { "isfile", fs_isfile },
    { "isdirectory", fs_isdirectory },
	{ "definealias", fs_definealias },
	{ "removealias", fs_removealias },
	{ "setunqualifiedroot", fs_setunqualifiedroot },
	{ "cwd", fs_cwd },
	{ "copy", fs_copy },
	{ "mkdir", fs_mkdir },
	{ "rename", fs_rename },
	{ "move", fs_move },
	{ "remove", fs_remove },
	{ "execute", fs_execute },
	{ "resolvepath", fs_resolvepath },
	{ "resolvepathabsolute", fs_resolvepathabsolute },
	{ "watch", fs_watch },
	{ "lastwritten", fs_lastwritten },

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
