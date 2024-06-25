#include<nljson.h>

#include"render/Material.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

std::vector<RenderMaterial*> RenderMaterial::s_loadedMaterials;
static const std::string MissingTexPath = "textures/MISSING2_MaximumADHD_status_1665776378145304579.png";

RenderMaterial::RenderMaterial()
{
	this->Name = "*ERROR*";
	this->HasSpecular = false;

	this->DiffuseTextures.push_back(new Texture());
	this->SpecularTextures.push_back(new Texture());

	this->DiffuseTextures[0]->ImagePath = "resources/" + MissingTexPath;
	this->SpecularTextures[0]->ImagePath = "resources/" + MissingTexPath;

	TextureManager::Get()->CreateTexture2D(
		this->DiffuseTextures[0]
	);
}

RenderMaterial::RenderMaterial(std::string MaterialName)
{
	this->Name = MaterialName;

	int CachedMaterialIdx = -1;

	for (int SearchIndex = 0; SearchIndex < RenderMaterial::s_loadedMaterials.size(); SearchIndex++)
	{
		if (RenderMaterial::s_loadedMaterials[SearchIndex]->Name == MaterialName)
		{
			CachedMaterialIdx = SearchIndex;
			break;
		}
	}

	// frickign cache thang no work
	if (CachedMaterialIdx == -1)
	{
		//const char* MaterialPath = ("./resources/materials/" + MaterialName + ".mtl");

		std::string FileData = FileRW::ReadFile("materials/" + MaterialName + ".mtl");

		nlohmann::json JSONMaterialData;

		if (FileData != "")
		{
			JSONMaterialData = nlohmann::json::parse(FileData);
		}
		else
		{
			JSONMaterialData["albedo"] = "";

			Debug::Log("Unknown material: '" + MaterialName + "'");
		}

		this->DiffuseTextures.push_back(new Texture());

		this->DiffuseTextures[0]->Usage = MaterialTextureType::Diffuse;

		bool HasSpecularTexture = JSONMaterialData.find("specular") != JSONMaterialData.end();
		this->HasSpecular = HasSpecularTexture;

		this->DiffuseTextures[0]->ImagePath = "resources/" + JSONMaterialData.value(
			"albedo",
			MissingTexPath
		);

		TextureManager::Get()->CreateTexture2D(
			this->DiffuseTextures[0]
		);

		if (HasSpecular)
		{
			this->SpecularTextures.push_back(new Texture());

			this->SpecularTextures[0]->Usage = MaterialTextureType::Specular;
			this->SpecularTextures[0]->ImagePath = "resources/" + JSONMaterialData.value(
				"specular",
				MissingTexPath
			);

			TextureManager::Get()->CreateTexture2D(
				this->SpecularTextures[0]
			);
		}

		this->SpecExponent = JSONMaterialData.value("specExponent", this->SpecExponent);
		this->SpecMultiply = JSONMaterialData.value("specMultiply", this->SpecMultiply);

		RenderMaterial::s_loadedMaterials.push_back(this);
	}
	else
	{
		RenderMaterial* CachedMaterial = RenderMaterial::s_loadedMaterials[CachedMaterialIdx];
		this->HasSpecular = CachedMaterial->HasSpecular;
		this->DiffuseTextures = CachedMaterial->DiffuseTextures;
		this->SpecularTextures = CachedMaterial->SpecularTextures;
	}
}
