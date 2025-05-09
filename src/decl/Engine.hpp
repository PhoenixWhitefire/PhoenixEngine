#pragma once

#include <filesystem>
#include <nljson.hpp>
#include <SDL3/SDL_video.h>

#include "render/Renderer.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/ShaderManager.hpp"
#include "asset/MeshProvider.hpp"
#include "ThreadManager.hpp"

#include "component/DataModel.hpp"
#include "component/Workspace.hpp"

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
	void Close();
	
	GameObjectRef DataModel{};
	GameObjectRef Workspace{};

	Renderer RendererContext;
	SDL_Window* Window{};

	EventSignal<double> OnFrameStart{};
	EventSignal<double> OnFrameRenderGui{};
	EventSignal<double> OnFrameEnd{};

	bool IsFullscreen = false;
	bool VSync = false;

	int WindowSizeX{};
	int WindowSizeY{};
	
	int FramesPerSecond = 0;
	int FpsCap = 60;

	bool DebugWireframeRendering = false;
	bool DebugAabbs = false;

	int ExitCode = 0;

private:
	int m_DrawnFramesInSecond = -1;
	bool m_IsRunning = false;

	ThreadManager m_ThreadManager;
	MaterialManager m_MaterialManager;
	TextureManager m_TextureManager;
	ShaderManager m_ShaderManager;
	MeshProvider m_MeshProvider;
	
	GpuFrameBuffer m_BloomFbo;
};
