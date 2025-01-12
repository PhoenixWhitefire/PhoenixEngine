#include <format>
#include <nljson.hpp>

#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "Profiler.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

static const std::string MissingTexPath = "!Missing";

void RenderMaterial::Reload()
{
	PROFILE_SCOPE("Material::Reload");

	bool matExists = true;
	std::string fileData = FileRW::ReadFile("materials/" + this->Name + ".mtl", &matExists);

	size_t jsonStartLoc = fileData.find("{");
	std::string jsonFileContents = fileData.substr(jsonStartLoc);

	nlohmann::json jsonMaterialData = {};

	ShaderManager* shdManager = ShaderManager::Get();

	if (matExists)
	{
		try
		{
			jsonMaterialData = nlohmann::json::parse(jsonFileContents);
		}
		catch (nlohmann::json::parse_error e)
		{
			std::string errmsg = e.what();
			Log::Error(std::vformat(
				"Parse error trying to load material {}: {}",
				std::make_format_args(this->Name, errmsg)
			));
		}
	}
	else
	{
		this->ShaderId = shdManager->LoadFromPath("error");

		Log::Error("Unknown material: '" + this->Name + "'");

		return;
	}

	TextureManager* texManager = TextureManager::Get();

	std::string desiredShp = jsonMaterialData.value("Shader", jsonMaterialData.value("shaderprogram", "worldUber"));
	this->ShaderId = shdManager->LoadFromPath(desiredShp);

	nlohmann::json uniforms = jsonMaterialData.value("Uniforms", jsonMaterialData.value("uniforms", nlohmann::json::object()));

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

	bool doBilinearFiltering = jsonMaterialData.value("BilinearFiltering", jsonMaterialData.value("bilinearFiltering", true));

	this->ColorMap = texManager->LoadTextureFromPath(jsonMaterialData.value(
		"ColorMap",
		jsonMaterialData.value("albedo", MissingTexPath)
	), true, doBilinearFiltering);

	std::string metallicRoughnessPath = jsonMaterialData.value("MetallicRoughnessMap", jsonMaterialData.value("specular", ""));
	std::string normalPath = jsonMaterialData.value("NormalMap", "");
	std::string emissionPath = jsonMaterialData.value("EmissionMap", "");

	if (metallicRoughnessPath != "")
		this->MetallicRoughnessMap = texManager->LoadTextureFromPath(metallicRoughnessPath, true, doBilinearFiltering);
	else
		this->MetallicRoughnessMap = texManager->LoadTextureFromPath("textures/black.png", true, doBilinearFiltering);

	if (normalPath != "")
		this->NormalMap = texManager->LoadTextureFromPath(normalPath, true, doBilinearFiltering);
	else
		this->NormalMap = 0;

	if (emissionPath != "")
		this->EmissionMap = texManager->LoadTextureFromPath(emissionPath, true, doBilinearFiltering);
	else
		this->EmissionMap = 0;

	this->HasTranslucency = jsonMaterialData.value("HasTranslucency", jsonMaterialData.value("translucency", false));

	this->SpecExponent = jsonMaterialData.value("specExponent", this->SpecExponent);
	this->SpecMultiply = jsonMaterialData.value("specMultiply", this->SpecMultiply);

	ShaderProgram& shader = GetShader();
	// reserved slots for material textures
	shader.SetUniform("ColorMap", 10);
	shader.SetUniform("MetallicRoughnessMap", 11);
	shader.SetUniform("NormalMap", 12);
	shader.SetUniform("EmissionMap", 13);
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

static MaterialManager* s_Instance = nullptr;

MaterialManager::~MaterialManager()
{
	if (s_Instance == this)
		s_Instance = nullptr;
}

void MaterialManager::Initialize()
{
	this->LoadMaterialFromPath("error");

	s_Instance = this;
}

MaterialManager* MaterialManager::Get()
{
	return s_Instance;
}

uint32_t MaterialManager::LoadMaterialFromPath(const std::string& Name)
{
	PROFILE_SCOPE("MaterialManager/LoadMaterialFromPath");

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
			Log::Error("Failed to load material '" + fullPath + "'");

			if (Name == "error")
				throw("Failed to load the 'error' material. It is required due to technical reasons (I'm lazy)");

			return this->LoadMaterialFromPath("error");
		}
	}
	else
		return it->second;
}

void MaterialManager::SaveToPath(const RenderMaterial& material, const std::string& Name)
{
	PROFILE_SCOPE("MaterialManager/SaveToPath");

	TextureManager* texManager = TextureManager::Get();

	const Texture& colorMap = texManager->GetTextureResource(material.ColorMap);
	const Texture& mrMap = texManager->GetTextureResource(material.MetallicRoughnessMap);
	const Texture& emissionMap = texManager->GetTextureResource(material.EmissionMap);
	const Texture& normalMap = texManager->GetTextureResource(material.NormalMap);

	nlohmann::json newMtlConfig{};

	newMtlConfig["ColorMap"] = colorMap.ImagePath;

	if (material.MetallicRoughnessMap != 0)
		newMtlConfig["MetallicRoughnessMap"] = mrMap.ImagePath;

	if (material.NormalMap != 0)
		newMtlConfig["NormalMap"] = normalMap.ImagePath;

	if (material.EmissionMap != 0)
		newMtlConfig["EmissionMap"] = emissionMap.ImagePath;

	newMtlConfig["specExponent"] = material.SpecExponent;
	newMtlConfig["specMultiply"] = material.SpecMultiply;
	newMtlConfig["HasTranslucency"] = material.HasTranslucency;
	newMtlConfig["Shader"] = material.GetShader().Name;

	newMtlConfig["Uniforms"] = {};

	for (auto& it : material.Uniforms)
	{
		const Reflection::GenericValue& value = it.second;

		switch (value.Type)
		{
		case (Reflection::ValueType::Bool):
		{
			newMtlConfig["Uniforms"][it.first] = value.AsBool();
			break;
		}
		case (Reflection::ValueType::Integer):
		{
			newMtlConfig["Uniforms"][it.first] = value.AsInteger();
			break;
		}
		case (Reflection::ValueType::Double):
		{
			newMtlConfig["Uniforms"][it.first] = static_cast<float>(value.AsDouble());
			break;
		}
		}
	}

	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::chrono::year_month_day ymd = std::chrono::floor<std::chrono::days>(now);

	std::string dateStr = std::to_string((uint32_t)ymd.day()) + "-"
		+ std::to_string((uint32_t)ymd.month()) + "-"
		+ std::to_string((int32_t)ymd.year());

	std::string filePath = "materials/" + Name + ".mtl";

	std::string fileContents = "PHNXENGI\n#Asset Material\n#Date " + dateStr + "\n\n" + newMtlConfig.dump(2);

	FileRW::WriteFile(
		filePath,
		fileContents,
		true
	);
}

RenderMaterial& MaterialManager::GetMaterialResource(uint32_t ResourceId)
{
	return m_Materials.at(ResourceId);
}

std::vector<RenderMaterial>& MaterialManager::GetLoadedMaterials()
{
	return m_Materials;
}
