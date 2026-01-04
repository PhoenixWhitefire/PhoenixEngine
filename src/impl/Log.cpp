#include <iostream>
#include <fstream>
#include <format>
#include <mutex>

#include "Log.hpp"
#include "component/EngineService.hpp"
#include "Utilities.hpp"
#include "FileRW.hpp"

static std::string ProgramLog = "";
static std::ofstream LogHandle;
static bool DidInit = false;

void Log::Save()
{
	if (!DidInit)
	{
		PHX_CHECK(FileRW::WriteFile("./log.txt", ""));
		LogHandle.open("./log.txt", std::ios_base::app);

		DidInit = true;
	}

	LogHandle << ProgramLog;
	LogHandle.close();
	LogHandle.open("./log.txt", std::ios_base::app);

	ProgramLog.clear();
}

static void appendToLog(const std::string_view& Message, bool NoNewline = false)
{
	static bool ThrewLogCapacityExceededException = false;

	if (ThrewLogCapacityExceededException)
		return;

	if ( (( Message.size() >= 2 && Message.substr(Message.size() - 2, 2) != "&&" )
		|| Message.size() < 2) && !NoNewline
	)
	{
		ProgramLog.append(Message);
		ProgramLog.append("\n");
		std::cout << Message << "\n";
	}
	else
	{
		std::string_view loggedString = !NoNewline ? Message.substr(0ull, Message.size() - 2) : Message;
		ProgramLog.append(loggedString);
		std::cout << loggedString;
	}

	// log > 2 megabytes... just in case something goes wrong
	// 10/11/2024
	if (ProgramLog.size() > 2e6)
	{
		ThrewLogCapacityExceededException = true;

		// Log Size Limit Exceeded Throwing Exception
		ProgramLog.append("\nLSLETE: Log size limit exceeded, throwing exception\n");
		Log::Save();

		RAISE_RT("Program log exceeds maximum size of 2e6 bytes (2 megabytes)");
	}
}

static bool s_WithinLogCallstack = false;

// Append message to log, which is saved to file every second
// and when app shuts down
// If the Message ends with `&&`, won't insert a newline automatically
// 11/11/2024
void Log::Append(const std::string_view& Message, const std::string_view& ExtraTags)
{
	if (ExtraTags != "")
		appendToLog(ExtraTags, true);
	appendToLog(Message, false);

	// If our logging becomes recursive then stop
	// TODO Preferably the events are deferred instead of never reaching
	// our callbacks
	if (!s_WithinLogCallstack)
	{
		s_WithinLogCallstack = true;
		EcEngine::SignalNewLogMessage(LogMessageType::None, Message, ExtraTags);
		s_WithinLogCallstack = false;
	}
}

void Log::Info(const std::string_view& Message, const std::string_view& ExtraTags)
{
	appendToLog("[INFO]", true);
	appendToLog(ExtraTags, true);
	appendToLog(": ", true);
	appendToLog(Message);

	if (!s_WithinLogCallstack)
	{
		s_WithinLogCallstack = true;
		EcEngine::SignalNewLogMessage(LogMessageType::Info, Message, ExtraTags);
		s_WithinLogCallstack = false;
	}
}

void Log::Warning(const std::string_view& Message, const std::string_view& ExtraTags)
{
	appendToLog("[WARN]", true);
	appendToLog(ExtraTags, true);
	appendToLog(": ", true);
	appendToLog(Message);

	if (!s_WithinLogCallstack)
	{
		s_WithinLogCallstack = true;
		EcEngine::SignalNewLogMessage(LogMessageType::Warning, Message, ExtraTags);
		s_WithinLogCallstack = false;
	}
}

void Log::Error(const std::string_view& Message, const std::string_view& ExtraTags)
{
	appendToLog("[ERRR]", true);
	appendToLog(ExtraTags, true);
	appendToLog(": ", true);
	appendToLog(Message);

	if (!s_WithinLogCallstack)
	{
		s_WithinLogCallstack = true;
		EcEngine::SignalNewLogMessage(LogMessageType::Error, Message, ExtraTags);
		s_WithinLogCallstack = false;
	}
}
