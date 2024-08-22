#include<fstream>
#include<format>

#include"FileRW.hpp"
#include"GlobalJsonConfig.hpp"
#include"Debug.hpp"

static std::string GetLocalizedFilePath(std::string const& NonLocalizedPath)
{
	std::string Path = NonLocalizedPath;

	// TODO: allow paths specified in resource files (eg materials and levels etc) to not have to start from the root
	// and start from resources/ automatically
	// currently, files that *are* from the root are specified by beginning the path with '/'
	// find a better method?
	try
	{
		if (Path[0] != '.')
			Path.insert(0, EngineJsonConfig.value("ResourcesDirectory", "resources/"));
	}
	catch (nlohmann::json::type_error e)
	{
		const char* whatStr = e.what();
		Debug::Log(std::vformat("Error localizing file path: '{}'", std::make_format_args(whatStr)));
	}

	return Path;
}

std::string FileRW::ReadFile(std::string const& FilePath, bool* DoesFileExist)
{
	std::string Path = std::string(FilePath);
	Path = GetLocalizedFilePath(Path);

	std::ifstream File(Path, std::ios::binary);

	std::string Contents = *new std::string();

	if (File)
	{
		if (DoesFileExist != nullptr)
			*DoesFileExist = true;

		File.seekg(0, std::ios::end);

		Contents.resize(File.tellg());
		File.seekg(0, std::ios::beg);

		File.read(&Contents[0], Contents.size());

		File.close();

		return Contents;
	}
	else
	{
		Debug::Log(std::vformat("Could not load file: '{}'", std::make_format_args(Path)));

		if (DoesFileExist != nullptr)
			*DoesFileExist = false;

		return Contents;
	}
}

void FileRW::WriteFile(const std::string& FilePath, const std::string& FileContents, bool InResourcesDirectory)
{
	std::string Path = InResourcesDirectory ? GetLocalizedFilePath(FilePath) : FilePath;

	Debug::Log("Writing file: '" + Path + "'...");

	std::ofstream File(Path.c_str());

	File << FileContents;

	File.close();
}
