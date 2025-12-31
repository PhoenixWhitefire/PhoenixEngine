#pragma once

#include <string>
#include <format>

namespace Log
{
	void Info(const std::string_view&, const std::string_view& ExtraTags = "");
	void Warning(const std::string_view&, const std::string_view& ExtraTags = "");
	void Error(const std::string_view&, const std::string_view& ExtraTags = "");
	void Append(const std::string_view&, const std::string_view& ExtraTags = "");
	void Save();

	template <typename ...Args>
	void InfoF(std::format_string<Args...> fmt, Args&&... args)
	{
		Info(std::format(fmt, std::forward<Args>(args)...));
	}

	template <typename ...Args>
	void WarningF(std::format_string<Args...> fmt, Args&&... args)
	{
		Warning(std::format(fmt, std::forward<Args>(args)...));
	}

	template <typename ...Args>
	void ErrorF(std::format_string<Args...> fmt, Args&&... args)
	{
		Error(std::format(fmt, std::forward<Args>(args)...));
	}

	template <typename ...Args>
	void AppendF(std::format_string<Args...> fmt, Args&&... args)
	{
		Append(std::format(fmt, std::forward<Args>(args)...));
	}
};
