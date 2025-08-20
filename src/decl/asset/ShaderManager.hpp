#pragma once

#include <unordered_map>

#include "asset/TextureManager.hpp"
#include "Reflection.hpp"
#include "Memory.hpp"

class ShaderProgram
{
public:
	// Set this Program as the active one, and flush
	// stale uniforms
	void Activate();
	void Reload();
	void Delete();
	void Save();

	// Mark a uniform to be updated upon `::Activate` being called,
	// to be set with the provided value
	void SetUniform(const std::string_view&, const Reflection::GenericValue&);
	// Sets a Texture Uniform to the Texture residing at the Resource ID Provided
	void SetTextureUniform(
		const std::string_view&,
		uint32_t,
		Texture::DimensionType Type = Texture::DimensionType::Texture2D
	);
	// THIS DOES NOT FLUSH THE UNIFORMS!
	// it acts just like `SetUniform`, but it's intended to reset the state
	// back to the default
	void ApplyDefaultUniforms();

	Memory::string<MEMCAT(Shader)> Name;
	Memory::string<MEMCAT(Shader)> VertexShader;
	Memory::string<MEMCAT(Shader)> FragmentShader;
	Memory::string<MEMCAT(Shader)> GeometryShader = "<NOT_SPECIFIED>";

	Memory::unordered_map<std::string, Reflection::GenericValue, MEMCAT(Shader)> DefaultUniforms;
	Memory::string<MEMCAT(Shader)> UniformsAncestor;

private:
	bool m_CheckForErrors(uint32_t Object, const char* Type);
	int32_t m_GetUniformLocation(const char*) const;

	uint32_t m_GpuId = UINT32_MAX;

	Memory::unordered_map<std::string, Reflection::GenericValue, MEMCAT(Shader)> m_PendingUniforms{};
};

class ShaderManager
{
public:
	void Shutdown();
	// USE SHUTDOWN!!
	~ShaderManager();

	void Initialize(bool IsHeadless);

	static ShaderManager* Get();

	Memory::vector<ShaderProgram, MEMCAT(Shader)>& GetLoadedShaders();

	uint32_t LoadFromPath(const std::string_view&);

	ShaderProgram& GetShaderResource(uint32_t);

	void ClearAll();
	void ReloadAll();

	bool IsHeadless = false;

private:
	Memory::vector<ShaderProgram, MEMCAT(Shader)> m_Shaders;
	Memory::unordered_map<Memory::string<MEMCAT(Shader)>, uint32_t, MEMCAT(Shader)> s_StringToShaderId;
};
