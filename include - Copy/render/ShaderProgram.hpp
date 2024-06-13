#pragma once

#include<render/GraphicsAbstractionLayer.hpp>

#include<Debug.hpp>
#include<FileRW.hpp>

class ShaderProgram
{
public:
	ShaderProgram(std::string VertexShaderPath, std::string FragmentShaderPath, std::string GeometryShaderPath = "");
	~ShaderProgram();

	void PrintErrors(unsigned int Object, const char* Type);

	void Activate();

	static std::string BaseShaderPath;
	static SDL_Window* Window;

	GLuint ID;
};
