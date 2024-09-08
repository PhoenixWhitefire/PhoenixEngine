#include<format>
#include<glad/gl.h>
#include<nljson.hpp>

#include"render/ShaderProgram.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

static const std::string BaseShaderPath = "shaders/";
std::unordered_map<std::string, ShaderProgram*> ShaderProgram::s_programs;

ShaderProgram::ShaderProgram(std::string const& ProgramName)
{
	this->Name = ProgramName;

	this->Reload();
}

void ShaderProgram::Reload()
{
	if (this->ID != UINT32_MAX)
		glDeleteProgram(this->ID);

	this->ID = glCreateProgram();

	bool ShpExists = true;
	std::string ShpContents = FileRW::ReadFile(BaseShaderPath + Name + ".shp", &ShpExists);

	if (!ShpExists)
	{
		if (Name == "error")
			throw("Cannot load the fallback Shader Program ('error.shp'), giving up.");

		// TODO: a different function for error logging
		// Should also fire a callback so that an Output can be implemented
		// 13/07/2024
		Debug::Log(std::vformat(
			"**ERR** Shader program '{}' does not exist! Geometry will appear magenta",
			std::make_format_args(Name))
		);

		ShaderProgram* fallback = ShaderProgram::GetShaderProgram("error");

		this->ID = fallback->ID;

		return;
	}

	nlohmann::json ShpJson = nlohmann::json::parse(ShpContents);

	bool HasGeometryShader = ShpJson.find("Geometry") != ShpJson.end();

	bool VertexShdExists = true;
	bool FragmentShdExists = true;
	bool GeometryShdExists = true;

	std::string VertexShaderPath = ShpJson.value("Vertex", "worldUber.vert");
	std::string FragmentShaderPath = ShpJson.value("Fragment", "worldUber.frag");
	std::string GeoShaderPath = ShpJson.value("Geometry", "");

	std::string VertexShaderStrSource = FileRW::ReadFile(BaseShaderPath + VertexShaderPath, &VertexShdExists);
	std::string FragmentShaderStrSource = FileRW::ReadFile(BaseShaderPath + FragmentShaderPath, &FragmentShdExists);
	std::string GeometryShaderStrSource = HasGeometryShader
											? FileRW::ReadFile(BaseShaderPath + GeoShaderPath, &GeometryShdExists)
											: "";

	if (!VertexShdExists)
		throw(std::vformat(
			"Could not load Vertex shader for program {}! File specified: {}",
			std::make_format_args(this->Name, VertexShaderPath)
		));

	if (!FragmentShdExists)
		throw(std::vformat(
			"Could not load Fragment shader for program {}! File specified: {}",
			std::make_format_args(this->Name, FragmentShaderPath)
		));

	if (!GeometryShdExists)
		throw(std::vformat(
			"Could not load Geometry shader for program {}! File specified: {}",
			std::make_format_args(this->Name, GeoShaderPath)
		));

	const char* VertexSource = VertexShaderStrSource.c_str();
	const char* FragmentSource = FragmentShaderStrSource.c_str();

	const char* GeometrySource = GeometryShaderStrSource.c_str();

	GLuint VertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// TODO: Make a default fallback Geometry Shader so I don't have to bother with this -
	// Looks a little too complicated rn tho
	// 13/07/2024
	GLuint GeometryShader = HasGeometryShader ? glCreateShader(GL_GEOMETRY_SHADER) : 0;

	glShaderSource(VertexShader, 1, &VertexSource, NULL);
	glShaderSource(FragmentShader, 1, &FragmentSource, NULL);

	if (HasGeometryShader)
	{
		glShaderSource(GeometryShader, 1, &GeometrySource, NULL);
		glCompileShader(GeometryShader);

		this->PrintErrors(GeometryShader, "geometry shader");
	}

	glCompileShader(VertexShader);
	glCompileShader(FragmentShader);

	this->PrintErrors(VertexShader, "vertex shader");
	this->PrintErrors(FragmentShader, "fragment shader");

	glAttachShader(this->ID, VertexShader);
	glAttachShader(this->ID, FragmentShader);

	if (HasGeometryShader)
		glAttachShader(this->ID, GeometryShader);

	glEnableVertexAttribArray(0);

	glLinkProgram(this->ID);

	this->PrintErrors(this->ID, "shader program");

	//free shader code from memory, they've already been compiled so aren't needed anymore
	glDeleteShader(VertexShader);
	glDeleteShader(FragmentShader);

	if (HasGeometryShader)
		glDeleteShader(GeometryShader);
}

void ShaderProgram::Activate() const
{
	glUseProgram(this->ID);
}

ShaderProgram::~ShaderProgram()
{
	auto it = std::find_if(
		s_programs.begin(),
		s_programs.end(),
		[this](auto&& p)
		{
			return p.second == this;
		}
	);

	if (it != s_programs.end())
	{
		glDeleteProgram(ID);
		s_programs.erase(it->first);
	}
	else
		Debug::Log(std::vformat(
			"Program (ID:{}) does not exist in the registry, not deleting.",
			std::make_format_args(ID)
		));
	
	ID = UINT32_MAX;
}

void ShaderProgram::PrintErrors(uint32_t Object, const char* Type) const
{
	char InfoLog[1024];

	if (Object != this->ID)
	{
		GLint HasCompiled;
		glGetShaderiv(Object, GL_COMPILE_STATUS, &HasCompiled);

		if (HasCompiled == GL_FALSE)
		{
			glGetShaderInfoLog(Object, 1024, NULL, InfoLog);

			std::string ErrorString = std::vformat(
				"Error while compiling {} for program {}:\n{}",
				std::make_format_args(Type, this->Name, InfoLog)
			);

			throw(ErrorString);
		}
	}
	else
	{
		int HasLinked;

		glGetProgramiv(Object, GL_LINK_STATUS, &HasLinked);

		if (HasLinked == GL_FALSE)
		{
			glGetProgramInfoLog(Object, 1024, NULL, InfoLog);

			throw(std::vformat("Error while linking shader program:\n{}", std::make_format_args(InfoLog)));
		}
	}
}

ShaderProgram* ShaderProgram::GetShaderProgram(std::string const& ProgramName)
{
	auto it = s_programs.find(ProgramName);

	if (it != s_programs.end())
		return it->second;
	else
	{
		ShaderProgram* NewProgram = new ShaderProgram(ProgramName);
		s_programs.insert(std::pair(ProgramName, NewProgram));

		return NewProgram;
	}
}

void ShaderProgram::ClearAll()
{
	s_programs.clear();
}

void ShaderProgram::ReloadAll()
{
	for (auto& it : s_programs)
		it.second->Reload();
}
