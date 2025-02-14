#pragma once

#include <string>

namespace Log
{
	void Info(const std::string_view&);
	void Warning(const std::string_view&);
	void Error(const std::string_view&);
	void Append(const std::string_view&);
	void Save();
};
