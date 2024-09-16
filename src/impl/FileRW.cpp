#include<filesystem>
#include<fstream>
#include<format>

#include"FileRW.hpp"
#include"GlobalJsonConfig.hpp"
#include"Debug.hpp"

// 16/09/2024 PWK2K
// https://stackoverflow.com/a/71658518
// Returns:
//   true upon success.
//   false upon failure, and set the std::error_code & err accordingly.
static bool createDirectoryRecursive(std::string const& dirName, std::error_code& err)
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

static std::string getLocalizedFilePath(const std::string& NonLocalizedPath)
{
	std::string path = NonLocalizedPath;

	// TODO: allow paths specified in resource files (eg materials and levels etc) to not have to start from the root
	// and start from resources/ automatically
	// currently, files that *are* from the root are specified by beginning the path with '/'
	// find a better method?
	try
	{
		if (path[0] != '.')
			path.insert(0, EngineJsonConfig.value("ResourcesDirectory", "resources/"));
	}
	catch (nlohmann::json::type_error e)
	{
		const char* whatStr = e.what();
		Debug::Log(std::vformat("Error localizing file path: '{}'", std::make_format_args(whatStr)));
	}

	return path;
}

std::string FileRW::ReadFile(std::string const& ShortPath, bool* DoesFileExist)
{
	std::string actualPath = getLocalizedFilePath(ShortPath);

	std::ifstream file(actualPath, std::ios::binary);

	std::string contents = *new std::string();

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
		if (DoesFileExist == nullptr)
			Debug::Log(std::vformat("Could not load file: '{}'", std::make_format_args(actualPath)));
		else
			*DoesFileExist = false;

		return contents;
	}
}

void FileRW::WriteFile(const std::string& ShortPath, const std::string& FileContents, bool InResourcesDirectory)
{
	std::string path = InResourcesDirectory ? getLocalizedFilePath(ShortPath) : ShortPath;

	std::ofstream file(path.c_str());

	if (file && file.is_open())
	{
		file << FileContents;
		file.close();
	}
	else
		throw(std::vformat(
			"FileRW::WriteFile could not open the handle to '{}'",
			std::make_format_args(path)
		));
}

void FileRW::WriteFileCreateDirectories(
	const std::string& ShortPath,
	const std::string& FileContents,
	bool InResourcesDirectory
)
{
	std::string path = InResourcesDirectory ? getLocalizedFilePath(ShortPath) : ShortPath;

	size_t containingDirLoc = path.find_last_of("/");
	std::string dirPath = path.substr(0, containingDirLoc);

	std::error_code ec;
	
	if (!createDirectoryRecursive(dirPath, ec))
		throw("`createDirectoryRecursive` failed: " + ec.message());

	FileRW::WriteFile(path, FileContents, false);
}

