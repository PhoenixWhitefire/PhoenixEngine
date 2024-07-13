#include<format>
#include<nljson.h>

#include"render/Material.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

std::unordered_map<std::string, RenderMaterial*> RenderMaterial::s_loadedMaterials;
static const std::string MissingTexPath = "textures/MISSING2_MaximumADHD_status_1665776378145304579.png";

RenderMaterial::RenderMaterial()
{
	this->Name = "*ERROR*";
	this->HasSpecular = false;

	this->DiffuseTextures.push_back(new Texture());
	this->SpecularTextures.push_back(new Texture());

	this->DiffuseTextures[0]->ImagePath = "resources/" + MissingTexPath;
	this->SpecularTextures[0]->ImagePath = "resources/" + MissingTexPath;

	this->Shader = ShaderProgram::GetShaderProgram("worldUber");

	TextureManager::Get()->CreateTexture2D(
		this->DiffuseTextures[0]
	);
}

RenderMaterial::RenderMaterial(std::string MaterialName)
{
	this->Name = MaterialName;

	bool matExists = true;
	std::string FileData = FileRW::ReadFile("materials/" + MaterialName + ".mtl", &matExists);

	nlohmann::json JSONMaterialData;

	if (matExists)
	{
		try
		{
			JSONMaterialData = nlohmann::json::parse(FileData);
		}
		catch (nlohmann::json::parse_error e)
		{
			std::string errmsg = e.what();
			throw(std::vformat("Parse error trying to load material {}: {}", std::make_format_args(MaterialName, errmsg)));
		}
	}
	else
	{
		JSONMaterialData["albedo"] = "";

		this->Shader = ShaderProgram::GetShaderProgram("error");

		Debug::Log("Unknown material: '" + MaterialName + "'");

		return;
	}

	std::string desiredShp = JSONMaterialData.value("shaderprogram", "worldUber");
	this->Shader = ShaderProgram::GetShaderProgram(desiredShp);

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
}

RenderMaterial* RenderMaterial::GetMaterial(std::string Name)
{
	auto it = RenderMaterial::s_loadedMaterials.find(Name);

	if (it == RenderMaterial::s_loadedMaterials.end())
	{
		bool matExists = true;
		std::string FileData = FileRW::ReadFile("materials/" + Name + ".mtl", &matExists);

		if (matExists)
		{
			RenderMaterial* newMat = new RenderMaterial(Name);
			RenderMaterial::s_loadedMaterials.insert(std::pair(Name, newMat));
			return newMat;
		}
		else
			return RenderMaterial::GetMaterial("err");
	}
	else
		return it->second;
}
