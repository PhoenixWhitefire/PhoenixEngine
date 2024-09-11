#include<format>
#include<glad/gl.h>
#include<nljson.hpp>
#include<glm/gtc/type_ptr.hpp>

#include"render/ShaderProgram.hpp"
#include"datatype/Vector3.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

static const std::string BaseShaderPath = "shaders/";

ShaderProgram::ShaderProgram(std::string const& ProgramName)
{
	this->Name = ProgramName;

	this->Reload();
}

void ShaderProgram::Activate() const
{
	glUseProgram(this->ID);
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
}

void ShaderProgram::SetUniformInt(const char* UniformName, int Value) const
{
	this->Activate();

	GLint location = glGetUniformLocation(this->ID, UniformName);
	glUniform1i(location, Value);
}

void ShaderProgram::SetUniformFloat(const char* UniformName, float Value) const
{
	this->Activate();

	GLint location = glGetUniformLocation(this->ID, UniformName);
	glUniform1f(location, Value);
}

void ShaderProgram::SetUniformFloat3(const char* UniformName, float X, float Y, float Z) const
{
	this->Activate();

	GLint location = glGetUniformLocation(this->ID, UniformName);
	glUniform3f(location, X, Y, Z);
}

void ShaderProgram::SetUniformMatrix(const char* UniformName, const glm::mat4& Matrix) const
{
	this->Activate();

	GLint location = glGetUniformLocation(this->ID, UniformName);
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(Matrix));
}

void ShaderProgram::SetUniform(const char* UniformName, const Reflection::GenericValue& Value) const
{
	this->Activate();

	GLint location = glGetUniformLocation(this->ID, UniformName);

	switch (Value.Type)
	{
	case (Reflection::ValueType::Bool):
	{
		glUniform1i(location, Value.Bool);
		break;
	}
	case (Reflection::ValueType::Integer):
	{
		glUniform1i(location, static_cast<int32_t>(Value.Integer));
		break;
	}
	case (Reflection::ValueType::Double):
	{
		glUniform1f(location, static_cast<float>(Value.Double));
		break;
	}
	case (Reflection::ValueType::Vector3):
	{
		Vector3 vec = Vector3(Value);
		glUniform3f(
			location,
			static_cast<float>(vec.X),
			static_cast<float>(vec.Y),
			static_cast<float>(vec.Z)
		);
		break;
	}
	case (Reflection::ValueType::Matrix):
	{
		glm::mat4 mat = Value.AsMatrix();
		glUniform4fv(location, 1, glm::value_ptr(mat));
		break;
	}

	default:
	{
		const std::string typeName = Reflection::TypeAsString(Value.Type);
		throw(std::vformat(
			"Unrecognized uniform type '{}' trying to set '{}' for program '{}'",
			std::make_format_args(typeName, UniformName, this->Name)
		));
	}
	}
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
