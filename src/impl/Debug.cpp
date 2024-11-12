#include "Debug.hpp"
#include "FileRW.hpp"

static std::string ProgramLog = "";

void Debug::Save()
{
	FileRW::WriteFile("log.txt", ProgramLog, false);
}

// Append message to log, which is saved to file every second
// and when app shuts down
// If the Message ends with `&&`, won't insert a newline automatically
// 11/11/2024
void Debug::Log(const std::string& Message)
{
	static bool ThrewLogCapacityExceededException = false;

	if (ThrewLogCapacityExceededException)
		return;

	if (Message.substr(Message.size() - 2, 2) != "&&")
	{
		ProgramLog.append(Message + "\n");
		printf((Message + "\n").c_str());
	}
	else
	{
		std::string loggedString = Message.substr(0ull, Message.size() - 2);
		ProgramLog.append(loggedString);
		printf(loggedString.c_str());
	}

	// log > 2 megabytes... just in case something goes wrong
	// 10/11/2024
	if (ProgramLog.size() > 2e6)
	{
		ThrewLogCapacityExceededException = true;

		// Log Size Limit Exceeded Throwing Exception
		ProgramLog.append("\nLSLETE: Log size limit exceeded, throwing exception\n");
		Debug::Save();

		throw("Program log exceeds maximum size of 2e6 bytes (2 megabytes)");
	}
}
