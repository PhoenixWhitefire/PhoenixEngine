#pragma once

#include<iostream>
#include<fstream>

#include"GlobalJsonConfig.hpp"
#include"Debug.hpp"

namespace FileRW
{
	/*
	Reads and returns the contents of a file

	@param const std::string& FilePath: The path of the file.
	@param OPTIONAL bool* DoesFileExist: Gets set to true or false depending on whether the file exists. ReadFile will always return an empty
	string if the file does not exist, hence this parameters' existance.
	*/
	std::string ReadFile(std::string FilePath, bool* DoesFileExist = nullptr);

	void WriteFile(const char* FilePath, std::string FileContents, bool InResourcesDirectory);
};
