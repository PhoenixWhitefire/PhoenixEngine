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

	nlohmann::json& uniforms = jsonMaterialData["uniforms"];

	for (auto memberIt = uniforms.begin(); memberIt != uniforms.end(); ++memberIt)
	{
		std::string uniformName = memberIt.key();
		const nlohmann::json& value = memberIt.value();

		switch (value.type())
		{
		case (nlohmann::detail::value_t::boolean):
		{
			m_UniformOverrides.insert(std::pair(uniformName, (bool)value));
			break;
		}
		case (nlohmann::detail::value_t::number_float):
		{
			m_UniformOverrides.insert(std::pair(uniformName, (float)value));
			break;
		}
		case (nlohmann::detail::value_t::number_integer):
		{
			m_UniformOverrides.insert(std::pair(uniformName, (int32_t)value));
			break;
		}
		case (nlohmann::detail::value_t::number_unsigned):
		{
			m_UniformOverrides.insert(std::pair(uniformName, (uint32_t)value));
			break;
		}

		default:
		{
			const char* typeName = value.type_name();

			throw(std::vformat(
				"Material '{}' tried to specify Uniform '{}', but it had unsupported type '{}'",
				std::make_format_args(this->Name, uniformName, typeName)
			));
			break;
		}
		}
	}

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

void RenderMaterial::ApplyUniformOverrides()
{
	for (auto& it : m_UniformOverrides)
		this->Shader->SetUniform(it.first.c_str(), it.second);
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
	mats.reserve(s_LoadedMaterials.size());

	for (auto& it : s_LoadedMaterials)
		mats.push_back(it.second);

	return mats;
}
