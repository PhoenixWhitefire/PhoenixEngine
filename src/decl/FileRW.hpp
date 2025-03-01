#pragma once

#include <string>
#include <vector>

namespace FileRW
{
	/*
	Reads and returns the contents of a file

	@param const std::string& FilePath: The path of the file. *IMPORTANT:* If it doesn't start with ".",
	it will automatically have "resources/" or whatever the resources directory is prepended to it (EXCEPT if 3rd arg is false).
	@param OPTIONAL bool* DoesFileExist: Gets set to true or false depending on whether the file exists.
	ReadFile will always return an empty string if the file does not exist, hence this parameters' existence.
	*/
	std::string ReadFile(const std::string_view&, bool* DoesFileExist = nullptr, bool PrependResDir = true);

	void WriteFile(
		const std::string_view& FilePath,
		const std::string_view& Contents,
		bool InResourcesDirectory,
		bool* SuccessPtr = nullptr
	);
	// Writes to a path, creating directories along the way if they do not exist
	void WriteFileCreateDirectories(
		const std::string_view& FilePath,
		const std::string_view& Contents,
		bool InResourcesDirectory,
		bool* SuccessPtr = nullptr
	);

	std::string TryMakePathCwdRelative(const std::string_view&);
};
