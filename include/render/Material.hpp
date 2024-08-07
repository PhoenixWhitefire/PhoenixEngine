#pragma once

#include<vector>
#include<unordered_map>

#include"render/TextureManager.hpp"

class RenderMaterial
{
public:

	explicit RenderMaterial();
	// Creates a completely new material from the given name.
	// SHOULD NOT BE USED DIRECTLY! USE RenderMaterial::GetMaterial INSTEAD!
	// 
	RenderMaterial(std::string MaterialName);
	// Fetches a material. Caches materials.
	static RenderMaterial* GetMaterial(std::string Name);

	std::string Name;

	std::vector<Texture*> DiffuseTextures;
	std::vector<Texture*> SpecularTextures;

	bool HasSpecular = false;
	float SpecExponent = 32.f;
	float SpecMultiply = 1.f;

private:

	static std::unordered_map<std::string, RenderMaterial*> s_loadedMaterials;
};
