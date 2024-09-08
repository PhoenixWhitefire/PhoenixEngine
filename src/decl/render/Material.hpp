#pragma once

#include<vector>
#include<unordered_map>

#include"render/TextureManager.hpp"
#include"render/ShaderProgram.hpp"

class RenderMaterial
{
public:
	// Fetches a material. Caches materials.
	static RenderMaterial* GetMaterial(std::string const& Name);
	static std::vector<RenderMaterial*> GetLoadedMaterials();

	std::string Name{};
	ShaderProgram* Shader{};

	Texture* DiffuseTexture{};
	Texture* SpecularTexture{};

	bool HasTranslucency = false;
	bool HasSpecular = false;
	float SpecExponent = 16.f;
	float SpecMultiply = .5f;

private:
	RenderMaterial();
	RenderMaterial(std::string const&);

	static inline std::unordered_map<std::string, RenderMaterial*> s_LoadedMaterials;
};
