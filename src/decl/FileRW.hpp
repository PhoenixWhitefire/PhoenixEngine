#pragma once

#include<string>

namespace FileRW
{
	/*
	Reads and returns the contents of a file

	@param const std::string& FilePath: The path of the file. *IMPORTANT:* If it doesn't start with ".",
	it will automatically have "resources/" or whatever the resources directory is prepended to it.
	@param OPTIONAL bool* DoesFileExist: Gets set to true or false depending on whether the file exists.
	ReadFile will always return an empty string if the file does not exist, hence this parameters' existence.
	*/
	std::string ReadFile(std::string const&, bool* DoesFileExist = nullptr);

	void WriteFile(
		const std::string& FilePath,
		const std::string& FileContents,
		bool InResourcesDirectory
	);
	// Writes to a path, creating directories along the way if they do not exist
	void WriteFileCreateDirectories(
		const std::string& FilePath,
		const std::string& FileContents,
		bool InResourcesDirectory
	);
};
