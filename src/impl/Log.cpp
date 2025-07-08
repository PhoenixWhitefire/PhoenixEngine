#include <iostream>
#include <fstream>
#include <format>
#include <mutex>

#include "Log.hpp"
#include "Utilities.hpp"
#include "FileRW.hpp"

static std::string ProgramLog = "";
static std::ofstream LogHandle;
static bool DidInit = false;
static std::mutex LogMutex;

void Log::Save()
{
	std::unique_lock<std::mutex> lock{ LogMutex };

	if (!DidInit)
	{
		FileRW::WriteFile("log.txt", "", false);
		LogHandle.open("log.txt", std::ios_base::app);

		DidInit = true;
	}

	LogHandle << ProgramLog;

	ProgramLog.clear();
}

static void logAppend(const std::string_view& Message, bool NoNewline = false, bool ManageMutex = false)
{
	std::unique_lock<std::mutex> lock;

	if (ManageMutex)
		lock = std::unique_lock<std::mutex>(LogMutex);

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

// Append message to log, which is saved to file every second
// and when app shuts down
// If the Message ends with `&&`, won't insert a newline automatically
// 11/11/2024
void Log::Append(const std::string_view& Message)
{
	logAppend(Message, false, true);
}

void Log::Info(const std::string_view& Message, const std::string_view& ExtraTags)
{
	std::unique_lock<std::mutex> lock{ LogMutex };

	logAppend("[INFO]", true);
	logAppend(ExtraTags, true);
	logAppend(": ", true);
	logAppend(Message);
}

void Log::Warning(const std::string_view& Message, const std::string_view& ExtraTags)
{
	std::unique_lock<std::mutex> lock{ LogMutex };

	logAppend("[WARN]", true);
	logAppend(ExtraTags, true);
	logAppend(": ", true);
	logAppend(Message);
}

void Log::Error(const std::string_view& Message, const std::string_view& ExtraTags)
{
	std::unique_lock<std::mutex> lock{ LogMutex };

	logAppend("[ERRR]", true);
	logAppend(ExtraTags, true);
	logAppend(": ", true);
	logAppend(Message);
}
