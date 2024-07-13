#pragma once

#include<unordered_map>
#include<SDL2/SDL_video.h>

class ShaderProgram
{
public:
	// DOES NOT CACHE PROGRAMS!
	// Use (static) ShaderProgram::GetShaderProgram instead!
	ShaderProgram(std::string ProgramName);
	~ShaderProgram();

	static ShaderProgram* GetShaderProgram(std::string ProgramName);
	static void ClearAll();

	void PrintErrors(uint32_t Object, const char* Type) const;

	void Activate() const;

	std::string Name;
	uint32_t ID;

private:
	static std::unordered_map<std::string, ShaderProgram*> s_programs;
};
