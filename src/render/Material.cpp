#include<format>
#include<nljson.hpp>

#include"render/Material.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

std::unordered_map<std::string, RenderMaterial*> RenderMaterial::s_loadedMaterials;
static const std::string MissingTexPath = "textures/MISSING2_MaximumADHD_status_1665776378145304579.png";

RenderMaterial::RenderMaterial()
{
	this->SpecMultiply = .5f;
	this->SpecExponent = 16.f;

	this->Name = "*ERROR*";
	this->HasSpecular = false;
	this->Translucency = false;

	this->Shader = ShaderProgram::GetShaderProgram("worldUber");

	this->DiffuseTextures.push_back(TextureManager::Get()->LoadTextureFromPath(MissingTexPath));
}

RenderMaterial::RenderMaterial(std::string const& MaterialName)
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

	this->DiffuseTextures.push_back(TextureManager::Get()->LoadTextureFromPath(JSONMaterialData.value(
		"albedo",
		MissingTexPath
	)));

	this->DiffuseTextures[0]->Usage = MaterialTextureType::Diffuse;

	bool HasSpecularTexture = JSONMaterialData.find("specular") != JSONMaterialData.end();
	this->HasSpecular = HasSpecularTexture;
	this->Translucency = JSONMaterialData.value("translucency", false);

	if (HasSpecular)
	{
		this->SpecularTextures.push_back(TextureManager::Get()->LoadTextureFromPath(JSONMaterialData.value(
			"specular",
			MissingTexPath
		)));

		this->SpecularTextures[0]->Usage = MaterialTextureType::Specular;
	}

	this->SpecExponent = JSONMaterialData.value("specExponent", this->SpecExponent);
	this->SpecMultiply = JSONMaterialData.value("specMultiply", this->SpecMultiply);
}

RenderMaterial* RenderMaterial::GetMaterial(std::string const& Name)
{
	auto it = RenderMaterial::s_loadedMaterials.find(Name);

	if (it == RenderMaterial::s_loadedMaterials.end())
	{
		std::string fullPath = "materials/" + Name + ".mtl";
		bool matExists = true;
		std::string FileData = FileRW::ReadFile(fullPath, &matExists);

		if (matExists)
		{
			RenderMaterial* newMat = new RenderMaterial(Name);
			RenderMaterial::s_loadedMaterials.insert(std::pair(Name, newMat));
			return newMat;
		}
		else
		{
			Debug::Log("Failed to load material '" + fullPath + "'");

			if (Name == "err")
				throw("Failed to load the 'err' material. It is required due to technical reasons (I'm lazy)");

			return RenderMaterial::GetMaterial("err");
		}
	}
	else
		return it->second;
}

std::vector<RenderMaterial*> RenderMaterial::GetLoadedMaterials()
{
	std::vector<RenderMaterial*> mats;

	for (auto& it : s_loadedMaterials)
		mats.push_back(it.second);

	return mats;
}
