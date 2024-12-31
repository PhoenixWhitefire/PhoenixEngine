#pragma once

#include <string>

namespace Log
{
	void Info(const std::string&);
	void Warning(const std::string&);
	void Error(const std::string&);
	void Append(const std::string&);
	void Save();
};
