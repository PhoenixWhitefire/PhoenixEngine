#include <iostream>
#include <format>

#include "Log.hpp"
#include "FileRW.hpp"

static std::string ProgramLog = "";

void Log::Save()
{
	FileRW::WriteFile("log.txt", ProgramLog, false);
}

// Append message to log, which is saved to file every second
// and when app shuts down
// If the Message ends with `&&`, won't insert a newline automatically
// 11/11/2024
void Log::Append(const std::string_view& Message)
{
	static bool ThrewLogCapacityExceededException = false;

	if (ThrewLogCapacityExceededException)
		return;

	if (Message.substr(Message.size() - 2, 2) != "&&")
	{
		ProgramLog.append(Message);
		ProgramLog.append("\n");
		std::cout << Message << "\n";
	}
	else
	{
		std::string_view loggedString = Message.substr(0ull, Message.size() - 2);
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

		throw("Program log exceeds maximum size of 2e6 bytes (2 megabytes)");
	}
}

void Log::Info(const std::string_view& Message)
{
	Log::Append("[INFO]: &&");
	Log::Append(Message);
}

void Log::Warning(const std::string_view& Message)
{
	Log::Append("[WARN]: &&");
	Log::Append(Message);
}

void Log::Error(const std::string_view& Message)
{
	Log::Append("[ERRR]: &&");
	Log::Append(Message);
}
