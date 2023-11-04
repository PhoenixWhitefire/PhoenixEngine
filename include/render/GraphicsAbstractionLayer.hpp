// TODO: actually implement this

#pragma once

#include<string>
#include<vector>

#include<datatype/Vector3.hpp>
#include<glm/gtc/type_ptr.hpp>

#include<Debug.hpp>

#include<glad/gl.h>

#include<SDL2/SDL.h>
#include<SDL2/SDL_syswm.h>

#include<imgui/imgui.h>
#include<imgui/imgui.h>
#include<imgui/imgui_impl_sdl.h>
#include<imgui/imgui_impl_opengl3.h>
//#include<imgui/imgui_impl_vulkan.h>

enum class GraphicsApi { Auto, None, OpenGL, Vulkan }; //None for saying that graphics has not been initialized.
enum class UniformType { INT, FLOAT, FLOAT3, MAT4 };
enum class GraphicsFeature { DEPTH_TESTING, BLENDING };

struct Uniform_t
{
	const char* Name;
	void* ValuePtr;
	unsigned int ShaderProgramId;
	UniformType Type;
};

class Graphics
{
public:
	/*
	Will initialize graphics and create a window.
	@param The graphics API to use - OpenGL, DirectX or let the function find the best supported one (Vulkan>OpenGL).
	Note forcing an API requires you to OR the correct flags into Graphics.WindowFlags.
	@param The title of the window.
	*/
	Graphics(
		bool* WasSuccess,
		GraphicsApi ForceApi = GraphicsApi::Auto,
		const char* WindowTitle = "PhoenixEngine",
		int WindowSizeX = 1400, int WindowSizeY = 700
	);

	void SetUniformBlock(std::vector<Uniform_t> Uniforms);
	void SetFeature(GraphicsFeature Feature, bool IsEnabled);

	SDL_Window* Window;

	//the graphics api that is being used (refer to enum GraphicsApi).
	GraphicsApi CurrentUsingApi = GraphicsApi::None;

	//SDL_WINDOW values OR'd together.
	uint32_t WindowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

	std::string Error;

	/*
	Destroys the graphics-related stuff that was created and closes the window.
	*/
	~Graphics();

private:
	//VkInstance VK_Instance;

	SDL_GLContext OGL_Context;

	ImGuiIO* GuiIO;
};
