#include<render/ShaderProgram.hpp>
#include<format>

std::string ShaderProgram::BaseShaderPath = "";
SDL_Window* ShaderProgram::Window = nullptr;

ShaderProgram::ShaderProgram(std::string VertexShaderPath, std::string FragmentShaderPath, std::string GeometryShaderPath)
{
	bool HasGeometryShader = GeometryShaderPath.length() > 0;

	std::string VertexShaderStrSource = FileRW::ReadFile(ShaderProgram::BaseShaderPath + VertexShaderPath);
	std::string FragmentShaderStrSource = FileRW::ReadFile(ShaderProgram::BaseShaderPath + FragmentShaderPath);
	std::string GeometryShaderStrSource = HasGeometryShader ? FileRW::ReadFile(ShaderProgram::BaseShaderPath + GeometryShaderPath) : "";

	if (VertexShaderStrSource == "")
		throw(std::vformat("Could not load VERTEX shader! File specified: {}", std::make_format_args(VertexShaderPath)));

	if (FragmentShaderStrSource == "")
		throw(std::vformat("Could not load FRAGMENT shader! File specified: {}", std::make_format_args(VertexShaderPath)));

	const char* VertexSource = VertexShaderStrSource.c_str();
	const char* FragmentSource = FragmentShaderStrSource.c_str();

	const char* GeometrySource = GeometryShaderStrSource.c_str();

	GLuint VertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
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

	this->ID = glCreateProgram();

	glAttachShader(this->ID, VertexShader);
	glAttachShader(this->ID, FragmentShader);
	
	if (HasGeometryShader) {
		glAttachShader(this->ID, GeometryShader);
	}
	
	glEnableVertexAttribArray(0);

	glLinkProgram(this->ID);

	this->PrintErrors(this->ID, "shader program");

	//free shader code from memory, they've already been compiled so aren't needed anymore
	glDeleteShader(VertexShader);
	glDeleteShader(FragmentShader);
	
	if (HasGeometryShader) {
		glDeleteShader(GeometryShader);
	}
}

void ShaderProgram::Activate()
{
	glUseProgram(this->ID);
}

ShaderProgram::~ShaderProgram()
{
	glDeleteProgram(this->ID);
}

void ShaderProgram::PrintErrors(unsigned int Object, const char* Type)
{

	char InfoLog[1024];

	if (Object != this->ID)
	{
		GLint HasCompiled;
		glGetShaderiv(Object, GL_COMPILE_STATUS, &HasCompiled);

		if (HasCompiled == GL_FALSE)
		{
			glGetShaderInfoLog(Object, 1024, NULL, InfoLog);

			std::string ErrorString = std::vformat("Error while compiling {}:\n{}", std::make_format_args(Type, InfoLog));

			throw(ErrorString);
		}
	}
	else
	{
		GLint HasLinked;

		glGetProgramiv(Object, GL_LINK_STATUS, &HasLinked);

		if (HasLinked == GL_FALSE)
		{
			glGetProgramInfoLog(Object, 1024, NULL, InfoLog);

			throw(std::vformat("Error while linking shader program:\n{}", std::make_format_args(InfoLog)));
		}
	}
}
