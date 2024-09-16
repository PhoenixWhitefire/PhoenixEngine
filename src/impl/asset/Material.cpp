#include<format>
#include<nljson.hpp>

#include"asset/Material.hpp"
#include"asset/TextureManager.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

static const std::string MissingTexPath = "textures/MISSING2_MaximumADHD_status_1665776378145304579.png";

RenderMaterial::RenderMaterial()
{
	this->Name = "*ERROR*";
	this->HasTranslucency = false;

	this->Shader = ShaderProgram::GetShaderProgram("worldUber");

	TextureManager* texManager = TextureManager::Get();

	this->ColorMap = texManager->LoadTextureFromPath(MissingTexPath);
	this->MetallicRoughnessMap = 0;
}

RenderMaterial::RenderMaterial(std::string const& MaterialName)
{
	this->Name = MaterialName;

	bool matExists = true;
	std::string fileData = FileRW::ReadFile("materials/" + MaterialName + ".mtl", &matExists);

	nlohmann::json jsonMaterialData;

	if (matExists)
	{
		try
		{
			jsonMaterialData = nlohmann::json::parse(fileData);
		}
		catch (nlohmann::json::parse_error e)
		{
			std::string errmsg = e.what();
			throw(std::vformat("Parse error trying to load material {}: {}", std::make_format_args(MaterialName, errmsg)));
		}
	}
	else
	{
		this->Shader = ShaderProgram::GetShaderProgram("error");

		Debug::Log("Unknown material: '" + MaterialName + "'");

		return;
	}

	TextureManager* texManager = TextureManager::Get();

	std::string desiredShp = jsonMaterialData.value("shaderprogram", "worldUber");
	this->Shader = ShaderProgram::GetShaderProgram(desiredShp);

	this->ColorMap = texManager->LoadTextureFromPath(jsonMaterialData.value(
		"albedo",
		MissingTexPath
	));

	bool hasSpecularTexture = jsonMaterialData.find("specular") != jsonMaterialData.end();
	this->HasTranslucency = jsonMaterialData.value("translucency", false);

	if (hasSpecularTexture)
		this->MetallicRoughnessMap = texManager->LoadTextureFromPath(jsonMaterialData.value(
			"specular",
			MissingTexPath
		));
	else
		this->MetallicRoughnessMap = 0;

	this->SpecExponent = jsonMaterialData.value("specExponent", this->SpecExponent);
	this->SpecMultiply = jsonMaterialData.value("specMultiply", this->SpecMultiply);
}

RenderMaterial* RenderMaterial::GetMaterial(std::string const& Name)
{
	auto it = s_LoadedMaterials.find(Name);

	if (it == s_LoadedMaterials.end())
	{
		std::string fullPath = "materials/" + Name + ".mtl";
		bool matExists = true;
		std::string FileData = FileRW::ReadFile(fullPath, &matExists);

		if (matExists)
		{
			RenderMaterial* newMat = new RenderMaterial(Name);
			s_LoadedMaterials.insert(std::pair(Name, newMat));

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

	for (auto& it : s_LoadedMaterials)
		mats.push_back(it.second);

	return mats;
}
