#if 0

#include <format>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <glm/gtc/type_ptr.hpp>
#include <glad/gl.h>

#include "render/GraphicsAbstractionLayer.hpp"
#include "Log.hpp"

GraphicsLayer::GraphicsLayer(
	bool* WasSuccess,
	GraphicsApi ForceApi,
	const char* WindowTitle,
	int WindowSizeX,
	int WindowSizeY
)
{
	this->OGL_Context = nullptr;
	this->GuiIO = nullptr;

	if (ForceApi == GraphicsApi::Auto)
	{
		this->CurrentApi = GraphicsApi::OpenGL;
		this->WindowFlags |= SDL_WINDOW_OPENGL;
	}
	else
		this->CurrentApi = ForceApi;

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

	switch (this->CurrentApi)
	{

	case GraphicsApi::OpenGL:
	{
		this->OGL_Context = SDL_GL_CreateContext(this->Window);

		if (!this->OGL_Context)
		{
			const char* SDLError = SDL_GetError();

			throw(std::vformat(
				"SDL could not create an OpenGL context: {}",
				std::make_format_args(SDLError)
			));
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

#endif
