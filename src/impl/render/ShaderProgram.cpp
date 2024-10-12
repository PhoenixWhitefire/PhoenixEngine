#include<format>
#include<glad/gl.h>
#include<nljson.hpp>
#include<glm/gtc/type_ptr.hpp>

#include"render/ShaderProgram.hpp"
#include"datatype/Vector3.hpp"
#include"datatype/Color.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

static const std::string BaseShaderPath = "shaders/";
static std::unordered_map<uint32_t, std::string> IdToShaderName;

static GLint getUniformLocationUnCached(uint32_t Program, const char* Name)
{
	GLint uniformLocation = glGetUniformLocation(Program, Name);

	if (uniformLocation < 0)
	{
		std::string& programName = IdToShaderName.at(Program);
		Debug::Log(std::vformat(
			"Tried to get the location of invalid uniform '{}' in shader program {} (ID:{})",
			std::make_format_args(Name, programName, Program)
		));
		return -1;
	}
	else
		return uniformLocation;
}

static GLint getUniformLocation(uint32_t Program, const std::string& Name)
{
	static std::unordered_map<uint32_t, std::unordered_map<std::string, GLint>> LocationCaches;

	auto shaderLocationCacheIt = LocationCaches.find(Program);

	if (shaderLocationCacheIt != LocationCaches.end())
	{
		auto cachedLocationIt = shaderLocationCacheIt->second.find(Name);

		if (cachedLocationIt != shaderLocationCacheIt->second.end())
			return cachedLocationIt->second;
		else
		{
			shaderLocationCacheIt->second[Name] = getUniformLocationUnCached(Program, Name.c_str());
			return getUniformLocation(Program, Name);
		}
	}
	else
	{
		LocationCaches[Program] = {};
		return getUniformLocation(Program, Name);
	}

	/*
	18/09/2024

	// This is what it used to look like:

	auto shaderLocationCacheIt = LocationCaches.find(Program);
	std::unordered_map<std::string, GLint> shaderLocationCache;

	// But, for some reason, this single line,
	// `shaderLocationCache = shaderLocationIt->second`,
	// Took `1 MILLISECOND`
	// EACH. CALL.
	// TO THIS FUNTION
	// WHY THE HELL IS THE COPY SO EXPENSIVE??
	// IT LITERALLY REDUCES THE FRAMERATE FROM 200 -> 20
	// 1000% REDUCTION
	// WHAT. IN. THE. HELL.

	if (shaderLocationCacheIt != LocationCaches.end())
		shaderLocationCache = shaderLocationCacheIt->second;

	auto cachedUniformLocationIt = shaderLocationCache.find(Name);
	GLint uniformLocation{};

	if (cachedUniformLocationIt != shaderLocationCache.end())
		uniformLocation = cachedUniformLocationIt->second;
	else
	{
		uniformLocation = glGetUniformLocation(Program, Name.c_str());
		if (uniformLocation < 0)
		{
			std::string& programName = IdToShaderName.at(Program);
			Debug::Log(std::vformat(
				"Tried to get the location of invalid uniform '{}' in shader program {} (ID:{})",
				std::make_format_args(Name, programName, Program)
			));
			return -1;
		}

		shaderLocationCache.insert(std::pair(Name, uniformLocation));
	}

	LocationCaches[Program] = shaderLocationCache;

	return uniformLocation;

	*/
}

ShaderProgram::ShaderProgram(std::string const& ProgramName)
{
	this->Name = ProgramName;

	this->Reload();
}

void ShaderProgram::Activate()
{
	glUseProgram(this->ID);

	for (auto& pair : m_PendingUniforms)
	{
		const std::string& uniformName = pair.first;
		const Reflection::GenericValue& value = pair.second;

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
	if (this->ID != UINT32_MAX)
		glDeleteProgram(this->ID);

	this->ID = glCreateProgram();

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

		ShaderProgram* fallback = ShaderProgram::GetShaderProgram("error");

		this->ID = fallback->ID;

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

	glAttachShader(this->ID, vertexShader);
	glAttachShader(this->ID, fragmentShader);

	if (hasGeometryShader)
		glAttachShader(this->ID, geometryShader);

	glEnableVertexAttribArray(0);

	glLinkProgram(this->ID);

	m_PrintErrors(this->ID, "shader program");

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

void ShaderProgram::ApplyDefaultUniforms()
{
	for (auto& it : m_DefaultUniforms)
		this->SetUniform(it.first.c_str(), it.second);
}

int32_t ShaderProgram::m_GetUniformLocation(const char* Uniform) const
{
	// 18/09/2024
	// ok so indexing just one hashmap seems to be slower than
	// literally just calling `glGetUniformLocation`
	return glGetUniformLocation(ID, Uniform);

	//auto usedIt = m_UsedUniforms.find(Uniform);
	//if (usedIt != m_UsedUniforms.end() && usedIt->second)
	//	return getUniformLocationUnCached(this->ID, Uniform); //getUniformLocation(this->ID, Uniform);
	//else
	//	return -1;
}
/*
void ShaderProgram::SetUniformInt(const char* UniformName, int Value)
{
	this->Activate();

	if (int32_t uniformLoc = m_GetUniformLocation(UniformName); uniformLoc > -1)
		glUniform1i(uniformLoc, Value);
}

void ShaderProgram::SetUniformFloat(const char* UniformName, float Value)
{
	this->Activate();

	if (int32_t uniformLoc = m_GetUniformLocation(UniformName); uniformLoc > -1)
		glUniform1f(uniformLoc, Value);
}

void ShaderProgram::SetUniformFloat3(const char* UniformName, float X, float Y, float Z)
{
	this->Activate();

	if (int32_t uniformLoc = m_GetUniformLocation(UniformName); uniformLoc > -1)
		glUniform3f(uniformLoc, X, Y, Z);
}

void ShaderProgram::SetUniformMatrix(const char* UniformName, const glm::mat4& Matrix)
{
	this->Activate();

	if (int32_t uniformLoc = m_GetUniformLocation(UniformName); uniformLoc > -1)
		glUniformMatrix4fv(uniformLoc, 1, GL_FALSE, glm::value_ptr(Matrix));
}
*/

void ShaderProgram::SetUniform(const char* UniformName, const Reflection::GenericValue& Value)
{
	m_PendingUniforms[UniformName] = Value;
}

void ShaderProgram::m_PrintErrors(uint32_t Object, const char* Type) const
{
	char infoLog[1024];

	if (Object != this->ID)
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

ShaderProgram::~ShaderProgram()
{
	auto it = std::find_if(
		s_Programs.begin(),
		s_Programs.end(),
		[this](auto&& p)
		{
			return p.second == this;
		}
	);

	if (it != s_Programs.end())
	{
		glDeleteProgram(ID);
		s_Programs.erase(it->first);
	}
	else
		Debug::Log(std::vformat(
			"Program (ID:{}) does not exist in the registry, not deleting.",
			std::make_format_args(ID)
		));
	
	ID = UINT32_MAX;
}

ShaderProgram* ShaderProgram::GetShaderProgram(std::string const& ProgramName)
{
	auto it = s_Programs.find(ProgramName);

	if (it != s_Programs.end())
		return it->second;
	else
	{
		ShaderProgram* newProgram = new ShaderProgram(ProgramName);
		s_Programs.insert(std::pair(ProgramName, newProgram));
		IdToShaderName[newProgram->ID] = ProgramName;

		return newProgram;
	}
}

void ShaderProgram::ClearAll()
{
	for (auto& programIt : s_Programs)
		delete programIt.second;

	s_Programs.clear();
}

void ShaderProgram::ReloadAll()
{
	for (auto& it : s_Programs)
		it.second->Reload();
}
