#pragma once

#include <string>
#include <vector>

namespace FileRW
{
	/*
		Reads and returns the contents of a file
		@param ShortPath The path to the file to read. "resources/" is automatically prepended to it, unless it begins with "."
			or an alias
		@param Success An optional pointer to a bool used to indicate whether the file was read succesfully. Returns an empty string if unsuccessful
		@param ErrorMessage An optional pointer to an string which will be filled with an error message upon failure
	*/
	std::string ReadFile(const std::string& ShortPath, bool* Success = nullptr, std::string* ErrorMessage = nullptr);

	bool WriteFile(
		const std::string& FilePath,
		const std::string_view& Contents,
		std::string* ErrorMessage = nullptr
	);
	// Writes to a path, creating directories along the way if they do not exist
	bool WriteFileCreateDirectories(
		const std::string& FilePath,
		const std::string_view& Contents,
		std::string* ErrorMessage = nullptr
	);

	// Defines an alias to be used in the beginning of a path
	// E.x.: with `::DefineAlias("modules", "scripts/modules")` the path
	// "@modules/MyModule.luau" resolves to "scripts/modules/MyModule.luau"
	void DefineAlias(const std::string& Alias, const std::string& Path);
	// When the path given begins with a `.` to indicate CWD, set the alias
	void MakeCwdAliasOf(const std::string&);

	std::string MakePathCwdRelative(std::string);
	std::string MakePathAbsolute(std::string);
};
