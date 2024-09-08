#include"Debug.hpp"
#include"FileRW.hpp"

static std::string ProgramLog = "";

void Debug::Save()
{
	FileRW::WriteFile("log.txt", ProgramLog, false);
}

void Debug::Log(std::string const& Message)
{
	ProgramLog.append(Message + "\n");

	printf((Message + "\n").c_str());
}
