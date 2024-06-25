#pragma once

#include<vector>

#include<render/TextureManager.hpp>


class RenderMaterial
{
public:
	explicit RenderMaterial();
	RenderMaterial(std::string MaterialName);

	std::string Name;

	std::vector<Texture*> DiffuseTextures;
	std::vector<Texture*> SpecularTextures;

	bool HasSpecular = false;
	float SpecExponent = 32.f;
	float SpecMultiply = 1.f;

private:
	static std::vector<RenderMaterial*> s_loadedMaterials;
};
