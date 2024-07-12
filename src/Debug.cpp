#include<string>

#include"Debug.hpp"
#include"FileRW.hpp"

std::string Debug::ProgramLog = "";

void Debug::Save()
{
	Debug::Log("Application closing...");

	FileRW::WriteFile("log.txt", Debug::ProgramLog, false);
}

void Debug::Log(std::string Message)
{
	Debug::ProgramLog.append(Message + "\n");

	printf((Message + "\n").c_str());
}
