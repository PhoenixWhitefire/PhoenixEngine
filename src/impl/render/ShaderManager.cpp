#include <format>
#include <glad/gl.h>
#include <nljson.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "render/ShaderManager.hpp"
#include "datatype/Vector3.hpp"
#include "datatype/Color.hpp"
#include "FileRW.hpp"
#include "Debug.hpp"

static const std::string BaseShaderPath = "shaders/";

void ShaderProgram::Activate()
{
	if (!glIsProgram(m_GpuId))
	{
		ShaderManager* shdManager = ShaderManager::Get();
		m_GpuId = shdManager->GetShaderResource(shdManager->LoadFromPath("error")).m_GpuId;

		if (!glIsProgram(m_GpuId))
		{
			Debug::Log(std::vformat(
				"Tried to ::Activate shader '{}', but it was (likely) deleted, and the fallback shader 'error' was also invalid.",
				std::make_format_args(this->Name)
			));

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
			case (Reflection::ValueType::Bool):
			{
				glUniform1i(uniformLoc, value.AsBool());
				break;
			}
			case (Reflection::ValueType::Integer):
			{
				glUniform1i(uniformLoc, static_cast<int32_t>(value.AsInteger()));
				break;
			}
			case (Reflection::ValueType::Double):
			{
				glUniform1f(uniformLoc, static_cast<float>(value.AsDouble()));
				break;
			}
			case (Reflection::ValueType::Vector3):
			{
				Vector3 vec = Vector3(value);
				glUniform3f(
					uniformLoc,
					static_cast<float>(vec.X),
					static_cast<float>(vec.Y),
					static_cast<float>(vec.Z)
				);
				break;
			}
			case (Reflection::ValueType::Color):
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
			case (Reflection::ValueType::Matrix):
			{
				glm::mat4 mat = value.AsMatrix();
				glUniformMatrix4fv(uniformLoc, 1, GL_FALSE, glm::value_ptr(mat));
				break;
			}

			default:
			{
				const std::string typeName = Reflection::TypeAsString(value.Type);
				throw(std::vformat(
					"Unrecognized uniform type '{}' trying to set '{}' for program '{}'",
					std::make_format_args(typeName, uniformName, this->Name)
				));
			}
			}
		}
	}

	m_PendingUniforms.clear();
}

void ShaderProgram::Reload()
{
	if (m_GpuId != UINT32_MAX)
		glDeleteProgram(m_GpuId);

	m_GpuId = glCreateProgram();

	bool shpExists = true;
	std::string shpContents = FileRW::ReadFile(BaseShaderPath + Name + ".shp", &shpExists);

	if (!shpExists)
	{
		if (this->Name == "error")
			throw("Cannot load the fallback Shader Program ('error.shp'), giving up.");

		// TODO: a different function for error logging
		// Should also fire a callback so that an Output can be implemented
		// 13/07/2024
		Debug::Log(std::vformat(
			"**ERR** Shader program '{}' does not exist! Geometry will appear magenta",
			std::make_format_args(this->Name))
		);

		ShaderManager* shpManager = ShaderManager::Get();
		ShaderProgram& fallback = shpManager->GetShaderResource(0);

		m_GpuId = fallback.m_GpuId;

		return;
	}

	nlohmann::json shpJson = nlohmann::json::parse(shpContents);

	bool hasGeometryShader = shpJson.find("Geometry") != shpJson.end();

	bool vertexShdExists = true;
	bool fragmentShdExists = true;
	bool geometryShdExists = true;

	std::string vertexShaderPath = shpJson.value("Vertex", "worldUber.vert");
	std::string fragmentShaderPath = shpJson.value("Fragment", "worldUber.frag");
	std::string geoShaderPath = shpJson.value("Geometry", "");

	std::string vertexShaderStrSource = FileRW::ReadFile(BaseShaderPath + vertexShaderPath, &vertexShdExists);
	std::string fragmentShaderStrSource = FileRW::ReadFile(BaseShaderPath + fragmentShaderPath, &fragmentShdExists);
	std::string geometryShaderStrSource = hasGeometryShader
											? FileRW::ReadFile(BaseShaderPath + geoShaderPath, &geometryShdExists)
											: "";

	if (!vertexShdExists)
		throw(std::vformat(
			"Could not load Vertex shader for program {}! File specified: {}",
			std::make_format_args(this->Name, vertexShaderPath)
		));

	if (!fragmentShdExists)
		throw(std::vformat(
			"Could not load Fragment shader for program {}! File specified: {}",
			std::make_format_args(this->Name, fragmentShaderPath)
		));

	if (!geometryShdExists)
		throw(std::vformat(
			"Could not load Geometry shader for program {}! File specified: {}",
			std::make_format_args(this->Name, geoShaderPath)
		));

	const char* vertexSource = vertexShaderStrSource.c_str();
	const char* fragmentSource = fragmentShaderStrSource.c_str();

	const char* geometrySource = geometryShaderStrSource.c_str();

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

		m_PrintErrors(geometryShader, "geometry shader");
	}

	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);

	m_PrintErrors(vertexShader, "vertex shader");
	m_PrintErrors(fragmentShader, "fragment shader");

	glAttachShader(m_GpuId, vertexShader);
	glAttachShader(m_GpuId, fragmentShader);

	if (hasGeometryShader)
		glAttachShader(m_GpuId, geometryShader);

	glEnableVertexAttribArray(0);

	glLinkProgram(m_GpuId);

	m_PrintErrors(m_GpuId, "shader program");

	//free shader code from memory, they've already been compiled so aren't needed anymore
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	if (hasGeometryShader)
		glDeleteShader(geometryShader);

	for (auto memberIt = shpJson["Uniforms"].begin(); memberIt != shpJson["Uniforms"].end(); ++memberIt)
	{
		const char* uniformName = memberIt.key().c_str();
		const nlohmann::json& value = memberIt.value();

		switch (value.type())
		{
		case (nlohmann::detail::value_t::boolean):
		{
			m_DefaultUniforms[uniformName] = (bool)value;
			break;
		}
		case (nlohmann::detail::value_t::number_float):
		{
			m_DefaultUniforms[uniformName] = (float)value;
			break;
		}
		case (nlohmann::detail::value_t::number_integer):
		{
			m_DefaultUniforms[uniformName] = (int32_t)value;
			break;
		}
		case (nlohmann::detail::value_t::number_unsigned):
		{
			m_DefaultUniforms[uniformName] = (uint32_t)value;
			break;
		}

		default:
		{
			const char* typeName = value.type_name();

			throw(std::vformat(
				"Shader Program '{}' tried to specify Uniform '{}', but it had unsupported type '{}'",
				std::make_format_args(this->Name, uniformName, typeName)
			));
			break;
		}
		}
	}

	this->ApplyDefaultUniforms();
}

void ShaderProgram::Delete()
{
	if (glIsProgram(m_GpuId))
		glDeleteProgram(m_GpuId);

	m_GpuId = UINT32_MAX;
}

void ShaderProgram::ApplyDefaultUniforms()
{
	for (auto& it : m_DefaultUniforms)
		this->SetUniform(it.first.c_str(), it.second);
}

int32_t ShaderProgram::m_GetUniformLocation(const char* Uniform) const
{
	return glGetUniformLocation(m_GpuId, Uniform);
}

void ShaderProgram::SetUniform(const std::string& UniformName, const Reflection::GenericValue& Value)
{
	m_PendingUniforms[UniformName] = Value;
}

void ShaderProgram::SetTextureUniform(const std::string& UniformName, uint32_t TextureId, Texture::DimensionType Type)
{
	static std::unordered_map<Texture::DimensionType, GLenum> DimensionTypeToGLDimension =
	{
		{ Texture::DimensionType::Texture2D, GL_TEXTURE_2D },
		{ Texture::DimensionType::Texture3D, GL_TEXTURE_3D },
		{ Texture::DimensionType::TextureCube, GL_TEXTURE_CUBE_MAP }
	};

	static TextureManager* texManager = TextureManager::Get();
	static uint32_t WhiteTextureId = texManager->LoadTextureFromPath("textures/white.png");
	static uint32_t WhiteTextureGpuId = texManager->GetTextureResource(WhiteTextureId).GpuId;

	if (TextureId == UINT32_MAX)
		TextureId = WhiteTextureGpuId;

	glActiveTexture(GL_TEXTURE0 + TextureId);
	glBindTexture(
		DimensionTypeToGLDimension.at(Type),
		TextureId
	);

	m_PendingUniforms[UniformName] = TextureId;
}

void ShaderProgram::m_PrintErrors(uint32_t Object, const char* Type) const
{
	char infoLog[1024];

	if (Object != m_GpuId)
	{
		GLint hasCompiled;
		glGetShaderiv(Object, GL_COMPILE_STATUS, &hasCompiled);

		if (hasCompiled == GL_FALSE)
		{
			glGetShaderInfoLog(Object, 1024, NULL, infoLog);

			std::string errorString = std::vformat(
				"Error while compiling {} for program '{}':\n{}",
				std::make_format_args(Type, this->Name, infoLog)
			);

			throw(errorString);
		}
	}
	else
	{
		int hasLinked;

		glGetProgramiv(Object, GL_LINK_STATUS, &hasLinked);

		if (hasLinked == GL_FALSE)
		{
			glGetProgramInfoLog(Object, 1024, NULL, infoLog);

			throw(std::vformat("Error while linking shader program:\n{}", std::make_format_args(infoLog)));
		}
	}
}

ShaderManager::ShaderManager()
{
	this->LoadFromPath("error");
}

ShaderManager* instance = nullptr;

ShaderManager* ShaderManager::Get()
{
	if (!instance)
		instance = new ShaderManager;
	return instance;
}

void ShaderManager::Shutdown()
{
	delete instance;
	instance = nullptr;
}

uint32_t ShaderManager::LoadFromPath(std::string const& ProgramName)
{
	auto it = s_StringToShaderId.find(ProgramName);

	if (it != s_StringToShaderId.end())
		return it->second;

	else
	{
		uint32_t resourceId = static_cast<uint32_t>(m_Shaders.size());
		s_StringToShaderId.emplace(ProgramName, resourceId);
		m_Shaders.emplace_back();

		ShaderProgram& shader = m_Shaders.back();
		shader.Name = ProgramName;
		shader.Reload();

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