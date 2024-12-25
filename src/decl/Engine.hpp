#pragma once

#include <filesystem>
#include <nljson.hpp>
#include <SDL2/SDL_video.h>

#include "gameobject/GameObjects.hpp"
#include "render/Renderer.hpp"

#include "datatype/Vector2.hpp"
#include "datatype/Event.hpp"
#include "PhysicsEngine.hpp"

// TODO: cleanup structure of public vs private
class EngineObject
{
public:
	~EngineObject();

	// Initialize Engine systems, can throw exceptions
	void Initialize();

	// Initializes main engine loop
	void Start();

	void SetIsFullscreen(bool IsFullscreen);
	// Resize to a different resolution, also runs `Engine->OnWindowResized`
	void ResizeWindow(int NewSizeX, int NewSizeY);
	// Handle changes to the window size. Exists in case the size changes from
	// something other than `ResizeWindow`
	void OnWindowResized(int NewSizeX, int NewSizeY);

	void LoadConfiguration();

	EventSignal<Reflection::GenericValue> OnFrameStart{};
	EventSignal<Reflection::GenericValue> OnFrameRenderGui{};
	EventSignal<Reflection::GenericValue> OnFrameEnd{};

	bool IsFullscreen = false;

	int WindowSizeX{};
	int WindowSizeY{};

	Object_DataModel* DataModel{};
	Object_Workspace* Workspace{};

	Renderer RendererContext;
	SDL_Window* Window{};

	bool Exit = false;
	
	int FpsCap = 60;

	int FramesPerSecond = 0;
	double FrameTime = 0.0f;

	bool VSync = false;

	double RunningTime = 0.0f;

private:
	int m_DrawnFramesInSecond = -1;

	uint32_t m_SDLWindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
};
