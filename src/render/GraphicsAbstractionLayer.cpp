#include<format>
#include<imgui/imgui_impl_sdl.h>
#include<imgui/imgui_impl_opengl3.h>
#include<SDL2/SDL_syswm.h>
#include<datatype/Vector3.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glad/gl.h>

#include"render/GraphicsAbstractionLayer.hpp"
#include"Debug.hpp"

Graphics::Graphics(bool* WasSuccess, GraphicsApi ForceApi, const char* WindowTitle, int WindowSizeX, int WindowSizeY)
{
	this->OGL_Context = nullptr;
	this->GuiIO = nullptr;

	if (ForceApi == GraphicsApi::Auto)
	{
		this->CurrentUsingApi = GraphicsApi::OpenGL;
		this->WindowFlags |= SDL_WINDOW_OPENGL;
	}
	else
		this->CurrentUsingApi = ForceApi;

	assert(this->CurrentUsingApi != GraphicsApi::None);

	this->Window = SDL_CreateWindow(
		WindowTitle,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		WindowSizeX, WindowSizeY,
		this->WindowFlags
	);

	if (!this->Window)
	{
		const char* SDLError = SDL_GetError();

		*WasSuccess = false;
		this->Error = std::string(SDLError);

		throw(std::vformat("SDL could not create window: {}", std::make_format_args(SDLError)));

		return;
	}

	switch (this->CurrentUsingApi) {

	case GraphicsApi::Vulkan:
	{
		throw(std::string("Vulkan is not currently supported :("));
		break;
	}

	case GraphicsApi::OpenGL:
	{
		this->OGL_Context = SDL_GL_CreateContext(this->Window);

		if (!this->OGL_Context)
		{
			const char* SDLError = SDL_GetError();

			throw(std::vformat("SDL could not create an OpenGL context: {}", std::make_format_args(SDLError)));

			*WasSuccess = false;
			this->Error = std::string(SDLError);

			return;
		}

		gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		SDL_GL_MakeCurrent(this->Window, this->OGL_Context);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		this->GuiIO = &ImGui::GetIO();
		ImGui::StyleColorsDark();
		ImGui_ImplSDL2_InitForOpenGL(this->Window, this->OGL_Context);
		ImGui_ImplOpenGL3_Init("#version 460");

		break;
	}

	default:
	{
		*WasSuccess = false;
		this->Error = "Forced specific graphics API but given invalid option";

		return;
	}

	}

	*WasSuccess = true;
}

static void GL_SetUniform(Uniform_t Uniform, SDL_Window* Window)
{
	switch (Uniform.Type)
	{

	case (UniformType::Integer):
	{
		glUseProgram(Uniform.ShaderProgramId);
		glUniform1i(glGetUniformLocation(Uniform.ShaderProgramId, Uniform.Name), *(int*)Uniform.ValuePtr);

		break;
	}

	case (UniformType::Float):
	{
		glUseProgram(Uniform.ShaderProgramId);
		glUniform1f(glGetUniformLocation(Uniform.ShaderProgramId, Uniform.Name), *(float*)Uniform.ValuePtr);

		break;
	}

	case (UniformType::Vector3):
	{
		glUseProgram(Uniform.ShaderProgramId);
		Vector3 Vec = *(Vector3*)Uniform.ValuePtr;
		glUniform3f(glGetUniformLocation(Uniform.ShaderProgramId, Uniform.Name), Vec.X, Vec.Y, Vec.Z);

		break;
	}

	case (UniformType::Matrix4x4):
	{
		glUseProgram(Uniform.ShaderProgramId);
		glUniformMatrix4fv(
			glGetUniformLocation(Uniform.ShaderProgramId, Uniform.Name),
			1,
			GL_FALSE,
			glm::value_ptr(*(glm::mat4*)Uniform.ValuePtr)
		);

		break;
	}

	default:
	{
		int Type = int(Uniform.Type);

		std::string ErrMsg = std::vformat(
			"GL_SetUniform not implemented for type {}!",
			std::make_format_args(Type)
		);

		throw(ErrMsg);
		break;
	}

	}
}

void Graphics::SetUniformBlock(std::vector<Uniform_t> Uniforms) const
{
	switch (this->CurrentUsingApi)
	{

	case (GraphicsApi::OpenGL):
	{
		for (int UniformIdx = 0; UniformIdx < Uniforms.size(); UniformIdx++)
			GL_SetUniform(Uniforms[UniformIdx], this->Window);
		break;
	}

	default: 
	{
		int curApi = int(this->CurrentUsingApi);

		std::string ErrMsg = std::vformat(
			"SetUniformBlock not implemented for API {}!",
			std::make_format_args(curApi)
		);

		throw(ErrMsg);
		break;
	}

	}
}

Graphics::~Graphics()
{
	SDL_DestroyWindow(this->Window);
}
