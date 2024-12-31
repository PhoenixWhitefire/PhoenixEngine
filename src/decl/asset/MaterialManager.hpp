#pragma once

#include <vector>
#include <unordered_map>

#include "render/ShaderManager.hpp"

struct RenderMaterial
{
	ShaderProgram& GetShader() const;
	// THIS DOES NOT FLUSH THE UNIFORMS!
	// It just does `SetUniform` for the uniforms in the Material's
	// JSON (like with `ShaderProgram::ApplyDefaultUniforms`)
	void ApplyUniforms();
	// Reload the material from File
	void Reload();

	std::string Name{};
	uint32_t ShaderId{};

	// Resource IDs for the `TextureManager` to resolve into a 
	// Graphics Texture ID
	uint32_t ColorMap{};
	uint32_t MetallicRoughnessMap{};
	uint32_t NormalMap{};
	uint32_t EmissionMap{};

	bool HasTranslucency = false;
	float SpecExponent{};
	float SpecMultiply{};

	std::unordered_map<std::string, Reflection::GenericValue> Uniforms;
};

class MaterialManager
{
public:
	static MaterialManager* Get();
	static void Shutdown();

	std::vector<RenderMaterial>& GetLoadedMaterials();

	//void FinalizeAsyncLoadedMaterials();

	uint32_t LoadMaterialFromPath(const std::string&);
	// uses material NAME, not file path!
	void SaveToPath(const RenderMaterial&, const std::string& Name);

	RenderMaterial& GetMaterialResource(uint32_t);

private:
	MaterialManager();

	std::vector<RenderMaterial> m_Materials;
	std::unordered_map<std::string, uint32_t> m_StringToMaterialId;

	/*
	std::vector<std::promise<RenderMaterial>*> m_MatPromises;
	std::vector<std::shared_future<RenderMaterial>> m_MatFutures;
	*/
};
