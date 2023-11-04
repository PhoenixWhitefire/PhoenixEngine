#pragma once

#include<vector>

#include<nljson.h>

#include<render/TextureManager.hpp>
#include<FileRW.hpp>

class Material {
	public:
		Material();

		Texture* Diffuse;
		Texture* Specular;

		bool HasSpecular;
};

class MaterialGetter {
	public:
		static MaterialGetter& Get();

		Material* GetMaterial(std::string MaterialName);

		std::vector<std::string> m_LoadedMaterialsPaths;
		std::vector<Material*> m_LoadedMaterials;

		static MaterialGetter* Singleton;
};
