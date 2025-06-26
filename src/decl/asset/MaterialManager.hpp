#pragma once

#include <vector>
#include <unordered_map>

#include "asset/ShaderManager.hpp"
#include "Memory.hpp"

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

	enum class MaterialPolygonMode : uint8_t
	{
		Fill = 0,
		Lines = 1,
		Points = 2
	};

	MaterialPolygonMode PolygonMode = MaterialPolygonMode::Fill;

	Memory::unordered_map<Memory::string<MEMCAT(Material)>, Reflection::GenericValue, MEMCAT(Material)> Uniforms;
};

class MaterialManager
{
public:
	void Shutdown();
	// USE SHUTDOWN!!
	~MaterialManager();

	void Initialize();

	static MaterialManager* Get();

	Memory::vector<RenderMaterial, MEMCAT(Material)>& GetLoadedMaterials();

	//void FinalizeAsyncLoadedMaterials();

	uint32_t LoadFromPath(const std::string_view&);
	// uses material NAME, not file path!
	void SaveToPath(const RenderMaterial&, const std::string_view& Name);

	RenderMaterial& GetMaterialResource(uint32_t);

private:
	Memory::vector<RenderMaterial, MEMCAT(Material)> m_Materials;
	Memory::unordered_map<std::string, uint32_t, MEMCAT(Material)> m_StringToMaterialId;

	/*
	std::vector<std::promise<RenderMaterial>*> m_MatPromises;
	std::vector<std::shared_future<RenderMaterial>> m_MatFutures;
	*/
};
