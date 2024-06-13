#pragma once

#include"FileRW.hpp"

class Debug {
public:
	static void Log(std::string Message);

	static void Save();

	static std::string ProgramLog;
};
