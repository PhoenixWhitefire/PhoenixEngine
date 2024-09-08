#pragma once

#include<unordered_map>
#include<SDL2/SDL_video.h>

class ShaderProgram
{
public:
	~ShaderProgram();

	static ShaderProgram* GetShaderProgram(std::string const&);
	static void ClearAll();
	static void ReloadAll();

	void PrintErrors(uint32_t Object, const char* Type) const;

	void Activate() const;

	void Reload();

	std::string Name;
	uint32_t ID = UINT32_MAX;

private:
	ShaderProgram() = delete;
	ShaderProgram(std::string const&);

	uint32_t m_VertexShader{};
	uint32_t m_FragmentShader{};
	uint32_t m_GeometryShader{};

	static std::unordered_map<std::string, ShaderProgram*> s_programs;
};
