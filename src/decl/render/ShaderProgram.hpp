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

	// Set this Program as the active one, and flush
	// stale uniforms
	void Activate();
	void Reload();

	/*
	void SetUniformInt(const char*, int);
	void SetUniformFloat(const char*, float);
	void SetUniformFloat3(const char*, float, float, float);
	void SetUniformMatrix(const char*, const glm::mat4&);
	*/

	// Mark a uniform to be updated upon `::Activate` being called,
	// to be set with the provided value
	void SetUniform(const char*, const Reflection::GenericValue&);

	std::string Name;
	uint32_t ID = UINT32_MAX;

private:
	ShaderProgram() = delete;
	ShaderProgram(std::string const&);

	void m_PrintErrors(uint32_t Object, const char* Type) const;
	int32_t m_GetUniformLocation(const char*);

	uint32_t m_VertexShader{};
	uint32_t m_FragmentShader{};
	uint32_t m_GeometryShader{};

	std::unordered_map<std::string, Reflection::GenericValue> m_PendingUniforms;

	static inline std::unordered_map<std::string, ShaderProgram*> s_Programs;
};
