#pragma once

#include <string>
#include <format>

namespace Logging
{
	void Save();
	void Initialize();
	void FlushParallelEvents();

	struct Context
	{
		void Info(const std::string_view&, const std::string_view& ExtraTags = "") const;
		void Warning(const std::string_view&, const std::string_view& ExtraTags = "") const;
		void Error(const std::string_view&, const std::string_view& ExtraTags = "") const;
		void Append(const std::string_view&, const std::string_view& ExtraTags = "") const;

		template <typename ...Args>
		void InfoF(std::format_string<Args...> fmt, Args&&... args) const
		{
			Info(std::format(fmt, std::forward<Args>(args)...));
		}

		template <typename ...Args>
		void WarningF(std::format_string<Args...> fmt, Args&&... args) const
		{
			Warning(std::format(fmt, std::forward<Args>(args)...));
		}

		template <typename ...Args>
		void ErrorF(std::format_string<Args...> fmt, Args&&... args) const
		{
			Error(std::format(fmt, std::forward<Args>(args)...));
		}

		template <typename ...Args>
		void AppendF(std::format_string<Args...> fmt, Args&&... args) const
		{
			Append(std::format(fmt, std::forward<Args>(args)...));
		}

		std::string ContextExtraTags;
	};

	struct LogMessageType_
	{
	    enum LMT {
	        None = 0,
	        Info,
	        Warning,
	        Error
	    };
	};

	using MessageType = LogMessageType_::LMT;
};

static inline Logging::Context Log;
