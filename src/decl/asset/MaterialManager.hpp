#pragma once

#include <vector>
#include <unordered_map>

#include "asset/ShaderManager.hpp"

struct RenderMaterial
{
	ShaderProgram& GetShader() const;
	// THIS DOES NOT FLUSH THE UNIFORMS!
	// It just does `SetUniform` for the uniforms in the Material's
	// JSON (like with `ShaderProgram::ApplyDefaultUniforms`)
	void ApplyUniforms();
	// Reload the material from File
	void Reload();

	std::string Name;
	uint32_t ShaderId = UINT32_MAX;

	// Resource IDs for the `TextureManager` to resolve into a 
	// Graphics Texture ID
	uint32_t ColorMap = 0;
	uint32_t MetallicRoughnessMap = 0;
	uint32_t NormalMap = 0;
	uint32_t EmissionMap = 0;

	bool HasTranslucency = false;
	float SpecExponent = 0.f;
	float SpecMultiply = 0.f;

	enum class MaterialPolygonMode : uint8_t
	{
		Fill = 0,
		Lines = 1,
		Points = 2
	};

	MaterialPolygonMode PolygonMode = MaterialPolygonMode::Fill;

	std::unordered_map<std::string, Reflection::GenericValue> Uniforms;
};

class MaterialManager
{
public:
	void Shutdown();
	// USE SHUTDOWN!!
	~MaterialManager();

	void Initialize();

	static MaterialManager* Get();

	std::vector<RenderMaterial>& GetLoadedMaterials();

	//void FinalizeAsyncLoadedMaterials();

	uint32_t LoadFromPath(const std::string_view&);
	// uses material NAME, not file path!
	void SaveToPath(const RenderMaterial&, const std::string_view& Name);

	RenderMaterial& GetMaterialResource(uint32_t);

private:
	std::vector<RenderMaterial> m_Materials;
	std::unordered_map<std::string, uint32_t> m_StringToMaterialId;

	/*
	std::vector<std::promise<RenderMaterial>*> m_MatPromises;
	std::vector<std::shared_future<RenderMaterial>> m_MatFutures;
	*/
};
