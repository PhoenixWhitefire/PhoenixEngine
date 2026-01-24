#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <format>
#include <tracy/public/tracy/Tracy.hpp>

#include "FileRW.hpp"
#include "GlobalJsonConfig.hpp"
#include "Utilities.hpp"
#include "Log.hpp"

// 16/09/2024 PWK2K
// https://stackoverflow.com/a/71658518
// Returns:
//   true upon success.
//   false upon failure, and set the std::error_code & err accordingly.
static bool createDirectoryRecursive(const std::string_view& dirName, std::error_code& err)
{
	ZoneScoped;

	err.clear();
	if (!std::filesystem::create_directories(dirName, err))
	{
		if (std::filesystem::exists(dirName))
		{
			// The folder already exists:
			err.clear();
			return true;
		}
		return false;
	}
	return true;
}

std::string FileRW::ReadFile(const std::string& ShortPath, bool* Success)
{
	ZoneScoped;

	const std::string actualPath = FileRW::MakePathCwdRelative(ShortPath);

	std::ifstream file;
	std::string contents;

	if (std::filesystem::is_regular_file(actualPath))
		file.open(actualPath, std::ios::binary);
	
	if (file && file.is_open())
	{
		if (Success != nullptr)
			*Success = true;

		file.seekg(0, std::ios::end);

		contents.resize(file.tellg());
		file.seekg(0, std::ios::beg);

		file.read(&contents[0], contents.size());

		file.close();

		return contents;
	}
	else
	{
		std::string shortError = std::strerror(errno);
		if (std::filesystem::is_directory(actualPath))
			shortError += " Not a file"; // "Succeeds" on Linux for some reason

		std::string fullError = std::format("Failed to read file '{}': {}", actualPath, shortError);
		Log::Error(fullError);

		if (Success)
			*Success = false;

		return fullError;
	}
}

bool FileRW::WriteFile(
	const std::string& ShortPath,
	const std::string_view& Contents,
	std::string* ErrorMessage
)
{
	ZoneScoped;

	std::string path = FileRW::MakePathCwdRelative(ShortPath);
	std::ofstream file(path.c_str(), std::ios::binary);

	if (file && file.is_open())
	{
		file.write((const char*)Contents.data(), Contents.size());
		file.close();

		return true;
	}
	else
	{
		std::string shortError = std::strerror(errno);
		std::string fullError = std::format("Failed to write {} bytes to file '{}': {}", Contents.size(), path, shortError);
		Log::Error(fullError);

		if (ErrorMessage)
			*ErrorMessage = fullError;

		return false;
	} 
}

bool FileRW::WriteFileCreateDirectories(
	const std::string& ShortPath,
	const std::string_view& Contents,
	std::string* ErrorMessage
)
{
	ZoneScoped;

	std::string path = FileRW::MakePathCwdRelative(ShortPath);

	size_t containingDirLoc = path.find_last_of("/");
	std::string dirPath = path.substr(0, containingDirLoc);

	std::error_code ec;
	
	if (!createDirectoryRecursive(dirPath, ec) && ErrorMessage)
	{
		*ErrorMessage = std::format("Failed to recursively create directories to '{}': {}", path, ec.message());
		return false;
	}

	return FileRW::WriteFile(path, Contents, ErrorMessage);
}

static std::unordered_map<std::string, std::string> s_AliasMap;
static std::string s_CwdAliasing;

void FileRW::DefineAlias(const std::string& Alias, const std::string& Path)
{
	assert(Alias[0] != '@'); // you've probably made a mistake
	s_AliasMap[Alias] = Path;
}

void FileRW::MakeCwdAliasOf(const std::string& Alias)
{
	s_CwdAliasing = Alias;
}

static std::string resolveAliasRecursive(std::string Path)
{
	size_t aliasEnd = Path.find_first_of('/');
	if (aliasEnd == std::string::npos)
		aliasEnd = Path.size();

	std::string alias = Path.substr(1, aliasEnd - 1);
	const auto& aliasIt = s_AliasMap.find(alias);

	if (aliasIt == s_AliasMap.end())
	{
		Log::ErrorF("Invalid alias '{}' in path '{}'", alias, Path);
		return Path;
	}
	else
		Path = aliasIt->second + Path.substr(aliasEnd, Path.size() - aliasEnd);

	if (Path[0] == '@')
		Path = resolveAliasRecursive(Path);
	
	return Path;
}

std::string FileRW::MakePathCwdRelative(std::string Path)
{
	if (Path.size() == 0)
	{
		Log::Warning("`MakePathCwdRelative` given a path 0 bytes in length!");
		return Path;
	}

	if (Path[0] == '!')
		return Path;

	if (Path[0] == '@')
		Path = resolveAliasRecursive(Path);

	std::string resdir = EngineJsonConfig.type() != nlohmann::json::value_t::null
							? EngineJsonConfig.value("ResourcesDirectory", "resources/")
							: "resources/";

	// TODO: allow paths specified in resource files (eg materials and levels etc) to not have to start from the root
	// and start from resources/ automatically
	// currently, files that *are* from the root are specified by beginning the path with `.` (such as `./`)
	// find a better method?
	// 12/01/2025: `.` is for preceding `./`, `:` is for drive letters, such as
	// `C:`, where we don't need to do anything
	// 23/02/2025: `/` is for Linux paths which begin at the home directory, starting with a `/home`
	// 19/08/2025: `~` is a shortcut for `/home/<USERNAME>` on linux
	size_t whereRes = Path.find(resdir);

	if ((Path[0] != '.' && Path[0] != '/' && Path[0] != '~' && (Path.size() < 2 || Path[1] != ':')))
	{
		if (whereRes == std::string::npos)
			Path.insert(0, resdir);

		if (s_CwdAliasing.size() > 0)
		{
			Path.insert(0, s_CwdAliasing);
			if (Path[0] == '@')
				Path = resolveAliasRecursive(Path);
		}
	}
	
	if (std::string cwd = std::filesystem::current_path().string(); Path.find(cwd) != std::string::npos)
		Path = Path.substr(cwd.size() + 1, Path.size() - cwd.size() - 1);

	if (Path[0] != '.' && Path[0] != '/' && Path[0] != '~' && (Path.size() < 2 || Path[1] != ':'))
		Path = "./" + Path;

	return Path;
}

std::string FileRW::MakePathAbsolute(std::string Path)
{
	if (Path.size() == 0)
	{
		Log::Warning("`MakePathAbsolute` given a path 0 bytes in length!");
		return Path;
	}

	std::string cwd = MakePathCwdRelative(Path);
	std::string abs = cwd;

	if (abs[0] != '/' && abs[0] != '~' && (abs.size() < 2 || abs[1] != ':'))
		abs = (std::filesystem::current_path() / abs).string();

#ifdef _WIN32
	for (char& c : abs)
		if (c == '/')
			c = '\\';
#endif

	return abs;
}
