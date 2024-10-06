// TODO: actually implement this

#pragma once

#include <string>
#include <vector>
#include <imgui/imgui.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_opengl.h>

enum class GraphicsApi { Auto, None, OpenGL }; //None for saying that graphics has not been initialized.
enum class GraphicsFeature { DepthTesting, TransparencyBlending };

class GraphicsLayer
{
public:
	/*
	Will initialize graphics and create a window.
	@param The graphics API to use - OpenGL, DirectX or let the function find the best supported one (Vulkan>OpenGL).
	Note forcing an API requires you to OR the correct flags into Graphics.WindowFlags.
	@param The title of the window.
	*/
	GraphicsLayer(
		bool* WasSuccess,
		GraphicsApi ForceApi = GraphicsApi::Auto,
		const char* WindowTitle = "PhoenixEngine",
		int WindowSizeX = 1400, int WindowSizeY = 700
	);

	void SetFeatureEnabled(GraphicsFeature Feature, bool IsEnabled);

	SDL_Window* Window;

	//the graphics api that is being used (refer to enum GraphicsApi).
	GraphicsApi CurrentApi = GraphicsApi::None;

	//SDL_WINDOW values OR'd together.
	uint32_t WindowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

	std::string Error;

	/*
	Destroys the graphics-related stuff that was created and closes the window.
	*/
	~GraphicsLayer();

private:
	//VkInstance VK_Instance;

	SDL_GLContext OGL_Context;

	ImGuiIO* GuiIO;
};

