#include <filesystem>
#include <fstream>
#include <format>

#include "FileRW.hpp"
#include "GlobalJsonConfig.hpp"
#include "Log.hpp"

// 16/09/2024 PWK2K
// https://stackoverflow.com/a/71658518
// Returns:
//   true upon success.
//   false upon failure, and set the std::error_code & err accordingly.
static bool createDirectoryRecursive(const std::string_view& dirName, std::error_code& err)
{
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

std::string FileRW::ReadFile(const std::string_view& ShortPath, bool* DoesFileExist, bool PrependResDir)
{
	std::string actualPath = PrependResDir ? FileRW::TryMakePathCwdRelative(ShortPath) : std::string(ShortPath);

	std::ifstream file(actualPath, std::ios::binary);

	std::string contents = "";

	if (file && file.is_open())
	{
		if (DoesFileExist != nullptr)
			*DoesFileExist = true;

		file.seekg(0, std::ios::end);

		contents.resize(file.tellg());
		file.seekg(0, std::ios::beg);

		file.read(&contents[0], contents.size());

		file.close();

		return contents;
	}
	else
	{
		if (!DoesFileExist)
			throw(std::vformat(
				"FileRW::ReadFile: Could not load file: '{}'",
				std::make_format_args(actualPath)
			));
		else
			*DoesFileExist = false;

		return contents;
	}
}

void FileRW::WriteFile(
	const std::string_view& ShortPath,
	const std::string_view& Contents,
	bool InResourcesDirectory,
	bool* SuccessPtr
)
{
	std::string path = InResourcesDirectory ? FileRW::TryMakePathCwdRelative(ShortPath) : std::string(ShortPath);

	std::ofstream file(path.c_str(), std::ios::binary);

	if (file && file.is_open())
	{
		file.write((char*)Contents.data(), Contents.size());
		file.close();
	}
	else
	{
		if (!SuccessPtr)
			throw(std::vformat(
				"FileRW::WriteFile: Could not open the handle to '{}'",
				std::make_format_args(path)
			));
		else
			*SuccessPtr = false;
	}
}

void FileRW::WriteFileCreateDirectories(
	const std::string_view& ShortPath,
	const std::string_view& Contents,
	bool InResourcesDirectory,
	bool* SuccessPtr
)
{
	std::string path = InResourcesDirectory ? FileRW::TryMakePathCwdRelative(ShortPath) : std::string(ShortPath);

	size_t containingDirLoc = path.find_last_of("/");
	std::string dirPath = path.substr(0, containingDirLoc);

	std::error_code ec;
	
	if (!createDirectoryRecursive(dirPath, ec))
		throw("FileRW::WriteFileCreateDirectories: `createDirectoryRecursive` failed: " + ec.message());

	FileRW::WriteFile(path, Contents, false, SuccessPtr);
}

std::string FileRW::TryMakePathCwdRelative(const std::string_view& LocalPath)
{
	std::string path = std::string(LocalPath);

	// TODO: allow paths specified in resource files (eg materials and levels etc) to not have to start from the root
	// and start from resources/ automatically
	// currently, files that *are* from the root are specified by beginning the path with `.` (such as `./`)
	// find a better method?
	// 12/01/2025: `.` is for preceding `./`, `:` is for drive letters, such as
	// `C:`, where we don't need to do anything
	// 23/02/2025: `/` is for Linux paths which begin at the home directory, starting with a `/home`
	size_t whereRes = path.find("resources/");

	if ((path[0] != '.' && path[0] != '/' && path[1] != ':') && whereRes == std::string::npos)
		path.insert(0, EngineJsonConfig.value("ResourcesDirectory", "resources/"));

	else if (whereRes != std::string::npos)
		// 23/02/2025: cut it off right to the `resources/` directory
		// prevent cases where an asset points to something outside of the CWD,
		// which can cause loading errors which are only present when a game is "exported"
		// or on a different machine
		path = path.substr(whereRes, path.size() - whereRes);

	return path;
}
