#pragma once

#include<unordered_map>

#include"Reflection.hpp"

class ShaderProgram
{
public:
	~ShaderProgram();

	static ShaderProgram* GetShaderProgram(std::string const&);
	static void ClearAll();
	static void ReloadAll();

	void Activate() const;
	void Reload();

	void SetUniformInt(const char*, int) const;
	void SetUniformFloat(const char*, float) const;
	void SetUniformFloat3(const char*, float, float, float) const;
	void SetUniformMatrix(const char*, const glm::mat4&) const;
	// Prefer the specialized setters compared to this one
	void SetUniform(const char*, const Reflection::GenericValue&) const;

	std::string Name;
	uint32_t ID = UINT32_MAX;

private:
	ShaderProgram() = delete;
	ShaderProgram(std::string const&);

	void m_PrintErrors(uint32_t Object, const char* Type) const;

	uint32_t m_VertexShader{};
	uint32_t m_FragmentShader{};
	uint32_t m_GeometryShader{};

	static inline std::unordered_map<std::string, ShaderProgram*> s_Programs;
};
