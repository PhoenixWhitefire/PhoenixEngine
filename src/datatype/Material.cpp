#include<datatype/Material.hpp>
#include<format>

MaterialGetter* MaterialGetter::Singleton = new MaterialGetter();

Material::Material()
{
	this->Diffuse = new Texture();
	this->Specular = new Texture();
}

MaterialGetter& MaterialGetter::Get() {
	return *MaterialGetter::Singleton;
}

Material* MaterialGetter::GetMaterial(std::string MaterialName)
{

	int LoadedMaterialIndex = -1;

	for (int SearchIndex = 0; SearchIndex < this->m_LoadedMaterialsPaths.size(); SearchIndex++)
	{
		if (this->m_LoadedMaterialsPaths[SearchIndex] == MaterialName)
		{
			LoadedMaterialIndex = SearchIndex;
			break;
		}
	}
	
	Material* LoadedMaterial = new Material();

	if (LoadedMaterialIndex == -1)
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

		LoadedMaterial->Diffuse->Usage = TextureType::DIFFUSE;

		bool HasSpecularTexture = JSONMaterialData.find("specular") != JSONMaterialData.end();
		LoadedMaterial->HasSpecular = HasSpecularTexture;

		LoadedMaterial->Specular->Usage = TextureType::SPECULAR;

		LoadedMaterial->Diffuse->ImagePath = "resources/" + JSONMaterialData.value("albedo", "textures/MISSING.png");

		TextureManager::Get()->CreateTexture2D(
			LoadedMaterial->Diffuse
		);

		if (HasSpecularTexture)
		{
			LoadedMaterial->Specular->ImagePath = (std::string)"resources/" + (std::string)JSONMaterialData["specular"];

			TextureManager::Get()->CreateTexture2D(
				LoadedMaterial->Specular
			);
		}
		else
			delete LoadedMaterial->Specular;
		
		this->m_LoadedMaterials.push_back(LoadedMaterial);
		this->m_LoadedMaterialsPaths.push_back(MaterialName);
	}
	else
		LoadedMaterial = this->m_LoadedMaterials[LoadedMaterialIndex];

	return LoadedMaterial;
}
