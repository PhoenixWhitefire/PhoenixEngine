#pragma once

#include<SDL2/SDL_video.h>

class ShaderProgram
{
public:
	ShaderProgram(std::string VertexShaderPath, std::string FragmentShaderPath, std::string GeometryShaderPath = "");
	~ShaderProgram();

	void PrintErrors(uint32_t Object, const char* Type);

	void Activate();

	static std::string BaseShaderPath;
	static SDL_Window* Window;

	uint32_t ID;
};
