#include <format>
#include <nljson.hpp>

#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "FileRW.hpp"
#include "Debug.hpp"

static const std::string MissingTexPath = "textures/missing.png";

void RenderMaterial::Reload()
{
	bool matExists = true;
	std::string fileData = FileRW::ReadFile("materials/" + this->Name + ".mtl", &matExists);

	nlohmann::json jsonMaterialData;

	ShaderManager* shdManager = ShaderManager::Get();

	if (matExists)
	{
		try
		{
			jsonMaterialData = nlohmann::json::parse(fileData);
		}
		catch (nlohmann::json::parse_error e)
		{
			std::string errmsg = e.what();
			throw(std::vformat(
				"Parse error trying to load material {}: {}",
				std::make_format_args(this->Name, errmsg)
			));
		}
	}
	else
	{
		this->ShaderId = shdManager->LoadFromPath("error");

		Debug::Log("Unknown material: '" + this->Name + "'");

		return;
	}

	TextureManager* texManager = TextureManager::Get();

	std::string desiredShp = jsonMaterialData.value("shaderprogram", "worldUber");
	this->ShaderId = shdManager->LoadFromPath(desiredShp);

	nlohmann::json& uniforms = jsonMaterialData["uniforms"];

	for (auto memberIt = uniforms.begin(); memberIt != uniforms.end(); ++memberIt)
	{
		std::string uniformName = memberIt.key();
		const nlohmann::json& value = memberIt.value();

		switch (value.type())
		{
		case (nlohmann::detail::value_t::boolean):
		{
			this->Uniforms.emplace(uniformName, (bool)value);
			break;
		}
		case (nlohmann::detail::value_t::number_float):
		{
			this->Uniforms.emplace(uniformName, (float)value);
			break;
		}
		case (nlohmann::detail::value_t::number_integer):
		{
			this->Uniforms.emplace(uniformName, (int32_t)value);
			break;
		}
		case (nlohmann::detail::value_t::number_unsigned):
		{
			this->Uniforms.emplace(uniformName, (uint32_t)value);
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

	bool doBilinearFiltering = jsonMaterialData.value("bilinearFiltering", true);

	this->ColorMap = texManager->LoadTextureFromPath(jsonMaterialData.value(
		"albedo",
		MissingTexPath
	), true, doBilinearFiltering);

	bool hasSpecularTexture = jsonMaterialData.find("specular") != jsonMaterialData.end();
	this->HasTranslucency = jsonMaterialData.value("translucency", false);

	if (hasSpecularTexture)
		this->MetallicRoughnessMap = texManager->LoadTextureFromPath(jsonMaterialData.value(
			"specular",
			MissingTexPath
		), true, doBilinearFiltering);
	else
		this->MetallicRoughnessMap = 0;

	this->SpecExponent = jsonMaterialData.value("specExponent", this->SpecExponent);
	this->SpecMultiply = jsonMaterialData.value("specMultiply", this->SpecMultiply);
}

ShaderProgram& RenderMaterial::GetShader() const
{
	return ShaderManager::Get()->GetShaderResource(this->ShaderId);
}

void RenderMaterial::ApplyUniforms()
{
	for (auto& it : Uniforms)
		this->GetShader().SetUniform(it.first.c_str(), it.second);
}

MaterialManager::MaterialManager()
{
	this->LoadMaterialFromPath("error");
}

MaterialManager* instance = nullptr;

MaterialManager* MaterialManager::Get()
{
	if (!instance)
		instance = new MaterialManager;
	return instance;
}

void MaterialManager::Shutdown()
{
	delete instance;
	instance = nullptr;
}

uint32_t MaterialManager::LoadMaterialFromPath(std::string const& Name)
{
	auto it = m_StringToMaterialId.find(Name);

	if (it == m_StringToMaterialId.end())
	{
		std::string fullPath = "materials/" + Name + ".mtl";
		bool matExists = true;
		std::string fileData = FileRW::ReadFile(fullPath, &matExists);

		if (matExists)
		{
			uint32_t resourceId = static_cast<uint32_t>(m_Materials.size());
			m_StringToMaterialId.emplace(Name, resourceId);
			m_Materials.emplace_back(Name);

			RenderMaterial& material = m_Materials.back();
			material.Name = Name;
			material.Reload();

			return resourceId;
		}
		else
		{
			Debug::Log("Failed to load material '" + fullPath + "'");

			if (Name == "error")
				throw("Failed to load the 'error' material. It is required due to technical reasons (I'm lazy)");

			return this->LoadMaterialFromPath("error");
		}
	}
	else
		return it->second;
}

RenderMaterial& MaterialManager::GetMaterialResource(uint32_t ResourceId)
{
	return m_Materials.at(ResourceId);
}

std::vector<RenderMaterial>& MaterialManager::GetLoadedMaterials()
{
	return m_Materials;
}