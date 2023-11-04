#include<render/GraphicsAbstractionLayer.hpp>
#include <format>

Graphics::Graphics(bool* WasSuccess, GraphicsApi ForceApi, const char* WindowTitle, int WindowSizeX, int WindowSizeY)
{
	if (ForceApi == GraphicsApi::Auto) {
		this->CurrentUsingApi = GraphicsApi::OpenGL;
		this->WindowFlags |= SDL_WINDOW_OPENGL;
	}
	else {
		this->CurrentUsingApi = ForceApi;
	}

	assert(this->CurrentUsingApi != GraphicsApi::None);

	this->Window = SDL_CreateWindow(
		WindowTitle,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		WindowSizeX, WindowSizeY,
		this->WindowFlags
	);

	if (!this->Window) {
		const char* SDLError = SDL_GetError();

		throw(std::vformat("SDL could not create window: {}", std::make_format_args(SDLError)));

		*WasSuccess = false;
		this->Error = std::string(SDLError);

		return;
	}

	switch (this->CurrentUsingApi) {

	case GraphicsApi::Vulkan: {
		throw(std::string("Vulkan is not currently supported :("));
		break;
	}

	case GraphicsApi::OpenGL: {
		this->OGL_Context = SDL_GL_CreateContext(this->Window);

		if (!this->OGL_Context) {
			const char* SDLError = SDL_GetError();

			throw(std::vformat("SDL could not create an OpenGL context: {}", std::make_format_args(SDLError)));

			*WasSuccess = false;
			this->Error = std::string(SDLError);

			return;
		}

		gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		SDL_GL_MakeCurrent(this->Window, this->OGL_Context);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		this->GuiIO = &ImGui::GetIO();
		ImGui::StyleColorsDark();
		ImGui_ImplSDL2_InitForOpenGL(this->Window, this->OGL_Context);
		ImGui_ImplOpenGL3_Init("#version 330");
	}

	default: {
		*WasSuccess = false;
		this->Error = "Forced specific graphics API but given invalid option";

		return;
	}

	}

	*WasSuccess = true;
}

void GL_SetUniform(Uniform_t Uniform, SDL_Window* Window)
{
	switch (Uniform.Type)
	{

	case (UniformType::INT):
	{
		glUseProgram(Uniform.ShaderProgramId);
		glUniform1i(glGetUniformLocation(Uniform.ShaderProgramId, Uniform.Name), *(int*)Uniform.ValuePtr);

		break;
	}

	case (UniformType::FLOAT):
	{
		glUseProgram(Uniform.ShaderProgramId);
		glUniform1f(glGetUniformLocation(Uniform.ShaderProgramId, Uniform.Name), *(float*)Uniform.ValuePtr);

		break;
	}

	case (UniformType::FLOAT3):
	{
		glUseProgram(Uniform.ShaderProgramId);
		Vector3 Vec = *(Vector3*)Uniform.ValuePtr;
		glUniform3f(glGetUniformLocation(Uniform.ShaderProgramId, Uniform.Name), Vec.X, Vec.Y, Vec.Z);

		break;
	}

	case (UniformType::MAT4):
	{
		glUseProgram(Uniform.ShaderProgramId);
		glUniformMatrix4fv(glGetUniformLocation(Uniform.ShaderProgramId, Uniform.Name), 1, GL_FALSE, glm::value_ptr(*(glm::mat4*)Uniform.ValuePtr));

		break;
	}

	default:
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine error", "GraphicsAbstractionLayer/GL_SetUniform not implemented for this uniform type!", Window);
		Debug::Log("GraphicsAbstractionLayer/GL_SetUniform not implemented for this uniform type!");
		Debug::Save();
		throw(std::vformat("GL_SetUniform does not have an implementation for type with ID {}", std::make_format_args((int)Uniform.Type)));
		break;
	}

	}
}

void Graphics::SetUniformBlock(std::vector<Uniform_t> Uniforms)
{
	switch (this->CurrentUsingApi)
	{

	case (GraphicsApi::OpenGL):
	{
		for (int UniformIdx = 0; UniformIdx < Uniforms.size(); UniformIdx++)
			GL_SetUniform(Uniforms[UniformIdx], this->Window);
	}

	default: 
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine error", "GraphicsAbstractionLayer::SetUniformBlock not implemented for this API!", this->Window);
		Debug::Log("GraphicsAbstractionLayer::SetUniformBlock not implemented for this API!");
		Debug::Save();
		throw(std::vformat("SetUniformBlock is not implemented for API with ID {}", std::make_format_args((int)this->CurrentUsingApi)));
		break;
	}

	}
}

Graphics::~Graphics()
{
	SDL_DestroyWindow(this->Window);
}
