#include<Debug.hpp>

std::string Debug::ProgramLog = "";

void Debug::Save() {
	Debug::Log("Application closing...");

	FileRW::WriteFile("/PROGRAMLOG.txt", Debug::ProgramLog, true);
}

void Debug::Log(std::string Message) {
	Debug::ProgramLog.append(Message + "\n");

	printf((Message + "\n").c_str());
}
