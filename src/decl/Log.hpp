#pragma once

#include <string>

namespace Log
{
	void Info(const std::string_view&, const std::string_view& ExtraTags = "");
	void Warning(const std::string_view&, const std::string_view& ExtraTags = "");
	void Error(const std::string_view&, const std::string_view& ExtraTags = "");
	void Append(const std::string_view&);
	void Save();
};
