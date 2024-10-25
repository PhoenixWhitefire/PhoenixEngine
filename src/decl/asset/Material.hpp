#pragma once

#include <vector>
#include <unordered_map>

#include "render/ShaderProgram.hpp"

class RenderMaterial
{
public:
	// Fetches a material. Caches materials.
	static RenderMaterial* GetMaterial(std::string const& Name);
	static std::vector<RenderMaterial*> GetLoadedMaterials();

	void ApplyUniforms();

	std::string Name{};
	ShaderProgram* Shader{};

	// Resource IDs for the `TextureManager` to resolve into a 
	// Graphics Texture ID
	uint32_t ColorMap{};
	uint32_t MetallicRoughnessMap{};
	uint32_t NormalMap{};

	bool HasTranslucency = false;
	float SpecExponent{};
	float SpecMultiply{};

	std::unordered_map<std::string, Reflection::GenericValue> Uniforms;

private:
	RenderMaterial();
	RenderMaterial(std::string const&);

	static inline std::unordered_map<std::string, RenderMaterial*> s_LoadedMaterials;
};
