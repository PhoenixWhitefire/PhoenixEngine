#pragma once

#include <unordered_map>

#include "asset/TextureManager.hpp"
#include "Reflection.hpp"

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

	std::string Name;
	std::string VertexShader;
	std::string FragmentShader;
	std::string GeometryShader = "<NOT_SPECIFIED>";

	std::unordered_map<std::string, Reflection::GenericValue> DefaultUniforms;
	std::string UniformsAncestor;

private:
	bool m_CheckForErrors(uint32_t Object, const char* Type);
	int32_t m_GetUniformLocation(const char*) const;

	uint32_t m_GpuId = UINT32_MAX;

	std::unordered_map<std::string, Reflection::GenericValue> m_PendingUniforms{};
};

class ShaderManager
{
public:
	~ShaderManager();

	void Initialize();

	static ShaderManager* Get();

	std::vector<ShaderProgram>& GetLoadedShaders();

	uint32_t LoadFromPath(const std::string_view&);

	ShaderProgram& GetShaderResource(uint32_t);

	void ClearAll();
	void ReloadAll();

private:
	std::vector<ShaderProgram> m_Shaders;
	std::unordered_map<std::string, uint32_t> s_StringToShaderId;
};
