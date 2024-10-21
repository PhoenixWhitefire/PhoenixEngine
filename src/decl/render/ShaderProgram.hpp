#pragma once

#include <unordered_map>

#include "Reflection.hpp"

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

	// Mark a uniform to be updated upon `::Activate` being called,
	// to be set with the provided value
	void SetUniform(const char*, const Reflection::GenericValue&);
	// THIS DOES NOT FLUSH THE UNIFORMS!
	// it acts just like `SetUniform`, but it's intended to reset the state
	// back to the default
	void ApplyDefaultUniforms();

	std::string Name;

private:
	ShaderProgram() = delete;
	ShaderProgram(std::string const&);

	void m_PrintErrors(uint32_t Object, const char* Type) const;
	int32_t m_GetUniformLocation(const char*) const;

	uint32_t m_GpuId = UINT32_MAX;

	uint32_t m_VertexShader{};
	uint32_t m_FragmentShader{};
	uint32_t m_GeometryShader{};

	std::unordered_map<std::string, Reflection::GenericValue> m_PendingUniforms;
	std::unordered_map<std::string, Reflection::GenericValue> m_DefaultUniforms;

	static inline std::unordered_map<std::string, ShaderProgram*> s_Programs;
};
