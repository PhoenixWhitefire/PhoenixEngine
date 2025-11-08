#include <format>
#include <glad/gl.h>
#include <nljson.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tracy/Tracy.hpp>

#include "asset/ShaderManager.hpp"

#include "asset/TextureManager.hpp"
#include "datatype/Color.hpp"
#include "Utilities.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

#define SP_TRYFALLBACK() { if (this->Name == "error")               \
	RAISE_RT("Fallback Shader failed to load");                     \
else                                                                \
	m_GpuId = ShaderManager::Get()->GetShaderResource(0).m_GpuId; } \

#define SP_LOADERROR(str) { Log::Error(str); SP_TRYFALLBACK(); return; }

static const std::string BaseShaderPath = "shaders/";

void ShaderProgram::Activate()
{
	ZoneScoped;

	if (ShaderManager::Get()->IsHeadless)
		return;

	if (!glIsProgram(m_GpuId))
	{
		ShaderManager* shdManager = ShaderManager::Get();
		m_GpuId = shdManager->GetShaderResource(shdManager->LoadFromPath("error")).m_GpuId;

		if (!glIsProgram(m_GpuId))
		{
			Log::ErrorF(
				"Tried to ::Activate shader '{}', but it was (likely) deleted, and the fallback shader 'error' was also invalid.",
				this->Name
			);

			return;
		}
	}

	glUseProgram(m_GpuId);

	for (auto& pair : m_PendingUniforms)
	{
		const std::string& uniformName = pair.first;
		Reflection::GenericValue& value = pair.second;

		if (int32_t uniformLoc = m_GetUniformLocation(uniformName.c_str()); uniformLoc > -1)
		{
			switch (value.Type)
			{
			case Reflection::ValueType::Boolean:
			{
				glUniform1i(uniformLoc, value.AsBoolean());
				break;
			}
			case Reflection::ValueType::Integer:
			{
				glUniform1i(uniformLoc, static_cast<int32_t>(value.AsInteger()));
				break;
			}
			case Reflection::ValueType::Double:
			{
				glUniform1f(uniformLoc, static_cast<float>(value.AsDouble()));
				break;
			}
			case Reflection::ValueType::Vector3:
			{
				glm::vec3& vec = value.AsVector3();
				glUniform3f(
					uniformLoc,
					vec.x,
					vec.y,
					vec.z
				);
				break;
			}
			case Reflection::ValueType::Color:
			{
				Color vec = Color(value);
				glUniform3f(
					uniformLoc,
					vec.R,
					vec.G,
					vec.B
				);
				break;
			}
			case Reflection::ValueType::Matrix:
			{
				glm::mat4 mat = value.AsMatrix();
				glUniformMatrix4fv(uniformLoc, 1, GL_FALSE, glm::value_ptr(mat));
				break;
			}

			[[unlikely]] default:
			{
				Log::WarningF(
					"Unrecognized uniform type '{}' trying to set '{}' for program '{}'",
					Reflection::TypeAsString(value.Type), uniformName, this->Name
				);
			}
			}
		}
	}

	m_PendingUniforms.clear();
}

void ShaderProgram::Reload()
{
	ZoneScoped;
	ZoneTextF("%s", this->Name.c_str());

	bool isHeadless = ShaderManager::Get()->IsHeadless;

	if (!isHeadless)
	{
		if (m_GpuId != UINT32_MAX)
			glDeleteProgram(m_GpuId);

		m_GpuId = glCreateProgram();
	}

	bool shpExists = true;
	std::string realShpPath = (Name.find("shaders/") == std::string::npos ? BaseShaderPath : "") + Name + ".shp";
	std::string shpContents = FileRW::ReadFile(
		realShpPath,
		&shpExists
	);

	if (!shpExists)
	{
		if (this->Name == "error")
			RAISE_RT("Cannot load the fallback Shader Program ('error.shp'), giving up.");

		// TODO: a different function for error logging
		// Should also fire a callback so that an Output can be implemented
		// 13/07/2024
		Log::ErrorF(
			"Shader program '{}' does not exist! Geometry will appear magenta",
			realShpPath
		);

		ShaderManager* shpManager = ShaderManager::Get();
		ShaderProgram& fallback = shpManager->GetShaderResource(0);

		m_GpuId = fallback.m_GpuId;

		return;
	}

	nlohmann::json shpJson = nlohmann::json::parse(shpContents);

	/*
		Certain SP's may just be another SP, but with a specific uniform
		(such as `worldUberTriProjected` really just being `worldUber` with
		the `UseTriPlanarProjection` uniform)

		Reduce duplicate configuration by allowing for "inheritance" of uniforms
		Do this *before* loading the SP-specific uniforms, so that inherited ones
		may be overriden
		Even means you can have lineages
	*/
	if (const auto uancestorIt = shpJson.find("InheritUniformsOf"); uancestorIt != shpJson.end())
	{
		UniformsAncestor = uancestorIt.value();

		if (UniformsAncestor != "")
		{
			ShaderManager* shdManager = ShaderManager::Get();

			uint32_t ancestorId = shdManager->LoadFromPath(UniformsAncestor);
			ShaderProgram& ancestor = shdManager->GetShaderResource(ancestorId);

			DefaultUniforms.insert(ancestor.DefaultUniforms.begin(), ancestor.DefaultUniforms.end());
		}
	}

	for (auto memberIt = shpJson["Uniforms"].begin(); memberIt != shpJson["Uniforms"].end(); ++memberIt)
	{
		const char* uniformName = memberIt.key().c_str();
		const nlohmann::json& value = memberIt.value();

		switch (value.type())
		{
		case (nlohmann::detail::value_t::boolean):
		{
			DefaultUniforms[uniformName] = (bool)value;
			break;
		}
		case (nlohmann::detail::value_t::number_float):
		{
			DefaultUniforms[uniformName] = (float)value;
			break;
		}
		case (nlohmann::detail::value_t::number_integer):
		{
			DefaultUniforms[uniformName] = (int32_t)value;
			break;
		}
		case (nlohmann::detail::value_t::number_unsigned):
		{
			DefaultUniforms[uniformName] = (uint32_t)value;
			break;
		}

		[[unlikely]] default:
		{
			Log::WarningF(
				"Shader Program '{}' tried to specify Uniform '{}', but it had unsupported type '{}'",
				this->Name, uniformName, value.type_name()
			);
			
			break;
		}
		}
	}

	this->ApplyDefaultUniforms();

	VertexShader = shpJson.value("Vertex", "worldUber.vert");
	FragmentShader = shpJson.value("Fragment", "worldUber.frag");
	GeometryShader = shpJson.value("Geometry", "<NOT_SPECIFIED>");

	VertexShader = (VertexShader.find("shaders/") == std::string::npos ? BaseShaderPath : "") + VertexShader;
	FragmentShader = (FragmentShader.find("shaders/") == std::string::npos ? BaseShaderPath : "") + FragmentShader;
	
	bool hasGeometryShader = GeometryShader != "<NOT_SPECIFIED>";

	if (hasGeometryShader)
		GeometryShader = (GeometryShader.find("shaders/") == std::string::npos ? BaseShaderPath : "") + GeometryShader;

	if (isHeadless)
		return;

	bool vertexShdExists = true;
	bool fragmentShdExists = true;
	bool geometryShdExists = true;

	std::string vertexStrSource = FileRW::ReadFile(VertexShader, &vertexShdExists);
	std::string fragmentStrSource = FileRW::ReadFile(FragmentShader, &fragmentShdExists);
	std::string geometryStrSource = hasGeometryShader
									? FileRW::ReadFile(GeometryShader, &geometryShdExists)
									: "";

	if (!vertexShdExists)
		SP_LOADERROR(std::format(
			"Could not load Vertex shader for program {}! File specified: {}",
			this->Name, VertexShader
		));

	if (!fragmentShdExists)
		SP_LOADERROR(std::format(
			"Could not load Fragment shader for program {}! File specified: {}",
			this->Name, FragmentShader
		));

	if (!geometryShdExists)
		SP_LOADERROR(std::format(
			"Could not load Geometry shader for program {}! File specified: {}",
			this->Name, GeometryShader
		));

	const char* vertexSource = vertexStrSource.c_str();
	const char* fragmentSource = fragmentStrSource.c_str();
	const char* geometrySource = geometryStrSource.c_str();

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// TODO: Make a default fallback Geometry Shader so I don't have to bother with this -
	// Looks a little too complicated rn tho
	// 13/07/2024
	GLuint geometryShader = hasGeometryShader ? glCreateShader(GL_GEOMETRY_SHADER) : 0;

	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);

	if (hasGeometryShader)
	{
		glShaderSource(geometryShader, 1, &geometrySource, NULL);
		glCompileShader(geometryShader);

		if (m_CheckForErrors(geometryShader, "geometry shader"))
			return;
	}

	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);

	if (m_CheckForErrors(vertexShader, "vertex shader"))
		return;

	if (m_CheckForErrors(fragmentShader, "fragment shader"))
		return;

	glAttachShader(m_GpuId, vertexShader);
	glAttachShader(m_GpuId, fragmentShader);

	if (hasGeometryShader)
		glAttachShader(m_GpuId, geometryShader);

	glEnableVertexAttribArray(0);

	glLinkProgram(m_GpuId);

	if (m_CheckForErrors(m_GpuId, "shader program"))
		return;

	//free shader code from memory, they've already been compiled so aren't needed anymore
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	if (hasGeometryShader)
		glDeleteShader(geometryShader);
}

void ShaderProgram::Delete()
{
	if (!ShaderManager::Get()->IsHeadless && glIsProgram(m_GpuId))
		glDeleteProgram(m_GpuId);

	m_GpuId = UINT32_MAX;
}

void ShaderProgram::Save()
{
	ZoneScoped;

	nlohmann::json fileJson{};

	fileJson["Vertex"] = VertexShader;
	fileJson["Fragment"] = FragmentShader;
	fileJson["Geometry"] = GeometryShader;

	fileJson["InheritUniformsOf"] = UniformsAncestor;

	for (auto& it : this->DefaultUniforms)
	{
		const Reflection::GenericValue& value = it.second;

		switch (value.Type)
		{
		case (Reflection::ValueType::Boolean):
		{
			fileJson["Uniforms"][it.first] = value.AsBoolean();
			break;
		}
		case (Reflection::ValueType::Integer):
		{
			fileJson["Uniforms"][it.first] = value.AsInteger();
			break;
		}
		case (Reflection::ValueType::Double):
		{
			fileJson["Uniforms"][it.first] = static_cast<float>(value.AsDouble());
			break;
		}

		[[unlikely]] default: { assert(false); }
		}
	}

	PHX_CHECK(FileRW::WriteFile(BaseShaderPath + Name + ".shp", fileJson.dump(2)));
}

void ShaderProgram::ApplyDefaultUniforms()
{
	for (auto& it : DefaultUniforms)
		this->SetUniform(it.first.c_str(), it.second);
}

int32_t ShaderProgram::m_GetUniformLocation(const char* Uniform) const
{
	assert(!ShaderManager::Get()->IsHeadless);
	return glGetUniformLocation(m_GpuId, Uniform);
}

void ShaderProgram::SetUniform(const std::string_view& UniformName, const Reflection::GenericValue& Value)
{
	m_PendingUniforms[std::string(UniformName)] = Value;
}

void ShaderProgram::SetTextureUniform(const std::string_view& UniformName, uint32_t TextureId, Texture::DimensionType Type)
{
	static std::unordered_map<Texture::DimensionType, GLenum> DimensionTypeToGLDimension =
	{
		{ Texture::DimensionType::Texture2D, GL_TEXTURE_2D },
		{ Texture::DimensionType::Texture3D, GL_TEXTURE_3D },
		{ Texture::DimensionType::TextureCube, GL_TEXTURE_CUBE_MAP }
	};

	static TextureManager* texManager = TextureManager::Get();
	static uint32_t WhiteTextureId = texManager->LoadTextureFromPath("!White");

	if (TextureId == 0)
		TextureId = WhiteTextureId;

	uint32_t gpuId = texManager->GetTextureResource(TextureId).GpuId;
	uint32_t slot = gpuId + 15; // the first 15 units are "reserved"

	m_PendingUniforms[std::string(UniformName)] = slot;

	if (ShaderManager::Get()->IsHeadless)
		return;

	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(
		DimensionTypeToGLDimension.at(Type),
		gpuId
	);
}

bool ShaderProgram::m_CheckForErrors(uint32_t Object, const char* Type)
{
	assert(!ShaderManager::Get()->IsHeadless);

	char infoLog[2048];

	if (Object != m_GpuId)
	{
		GLint hasCompiled;
		glGetShaderiv(Object, GL_COMPILE_STATUS, &hasCompiled);

		if (hasCompiled == GL_FALSE)
		{
			glGetShaderInfoLog(Object, 2048, NULL, infoLog);

			std::string errorString = std::format(
				"Error while compiling {} for program '{}':\n{}",
				Type, this->Name, infoLog
			);

			Log::Error(errorString);

			if (this->Name == "error")
				RAISE_RT("Failed to compile the required `error` Shader Pipeline");

			ShaderManager* shdManager = ShaderManager::Get();

			m_GpuId = shdManager->GetShaderResource(shdManager->LoadFromPath("error")).m_GpuId;

			return true;
		}
	}
	else
	{
		int hasLinked;

		glGetProgramiv(Object, GL_LINK_STATUS, &hasLinked);

		if (hasLinked == GL_FALSE)
		{
			glGetProgramInfoLog(Object, 2048, NULL, infoLog);

			Log::ErrorF(
				"Error while linking shader program '{}':\n{}",
				this->Name, infoLog
			);

			if (this->Name == "error")
				RAISE_RT("Failed to link the required `error` Shader Pipeline");

			ShaderManager* shdManager = ShaderManager::Get();

			m_GpuId = shdManager->GetShaderResource(shdManager->LoadFromPath("error")).m_GpuId;

			return true;
		}
	}

	return false;
}

static ShaderManager* s_Instance = nullptr;

void ShaderManager::Shutdown()
{
	s_Instance = nullptr;
}

ShaderManager::~ShaderManager()
{
	assert(!s_Instance);
}

void ShaderManager::Initialize(bool InitIsHeadless)
{
	ZoneScoped;

	this->IsHeadless = InitIsHeadless;
	s_Instance = this;
}

ShaderManager* ShaderManager::Get()
{
	assert(s_Instance);
	return s_Instance;
}

std::vector<ShaderProgram>& ShaderManager::GetLoadedShaders()
{
	return m_Shaders;
}

uint32_t ShaderManager::LoadFromPath(const std::string_view& ProgramName)
{
	auto it = s_StringToShaderId.find(std::string(ProgramName));

	if (it != s_StringToShaderId.end())
		return it->second;

	else
	{
		uint32_t resourceId = static_cast<uint32_t>(m_Shaders.size());
		s_StringToShaderId.emplace(ProgramName, resourceId);
		m_Shaders.emplace_back();

		// DON'T DO `m_Shaders.back` because `::Reload` could cause
		// `m_Shaders` to get re-allocated, causing it to be garbage data
		// 28/11/2024
		ShaderProgram shader{};
		shader.Name = ProgramName;
		shader.Reload();

		m_Shaders[resourceId] = shader;

		return resourceId;
	}
}

ShaderProgram& ShaderManager::GetShaderResource(uint32_t ResourceId)
{
	return m_Shaders.at(ResourceId);
}

void ShaderManager::ClearAll()
{
	for (ShaderProgram& shader : m_Shaders)
		shader.Delete();

	m_Shaders.clear();
	s_StringToShaderId.clear();
}

void ShaderManager::ReloadAll()
{
	for (ShaderProgram& shader : m_Shaders)
		shader.Reload();
}
