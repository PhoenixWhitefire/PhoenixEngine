#pragma once

#include <filesystem>
#include <nljson.hpp>
#include <SDL3/SDL_video.h>

#include "render/Renderer.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/ShaderManager.hpp"
#include "asset/MeshProvider.hpp"

#include "gameobject/DataModel.hpp"
#include "gameobject/Workspace.hpp"

#include "datatype/Vector2.hpp"
#include "datatype/Event.hpp"
#include "PhysicsEngine.hpp"

// TODO: cleanup structure of public vs private
class Engine
{
public:
	Engine();
	~Engine();

	static Engine* Get();

	// Initializes main engine loop
	void Start();

	void SetIsFullscreen(bool IsFullscreen);
	// Resize to a different resolution, also runs `Engine->OnWindowResized`
	void ResizeWindow(int NewSizeX, int NewSizeY);
	// Handle changes to the window size. Exists in case the size changes from
	// something other than `ResizeWindow`
	void OnWindowResized(int NewSizeX, int NewSizeY);

	void LoadConfiguration();

	EventSignal<double> OnFrameStart{};
	EventSignal<double> OnFrameRenderGui{};
	EventSignal<double> OnFrameEnd{};

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

	bool VSync = false;

	double RunningTime = 0.f;

private:
	int m_DrawnFramesInSecond = -1;

	uint32_t m_SDLWindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

	MaterialManager m_MaterialManager;
	TextureManager m_TextureManager;
	ShaderManager m_ShaderManager;
	MeshProvider m_MeshProvider;

	GameObjectRef<Object_DataModel>* m_DataModelRef = nullptr;
};
