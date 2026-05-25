#pragma once

#include <filesystem>
#include <nljson.hpp>
#include <imgui/imgui.h>

#include "render/Renderer.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/ShaderManager.hpp"
#include "asset/MeshProvider.hpp"
#include "ThreadManager.hpp"
#include "History.hpp"

#include "datatype/GameObject.hpp"
#include "datatype/Components.hpp"

#include "datatype/Event.hpp"
#include "geometry/Physics.hpp"

// TODO: cleanup structure of public vs private
class Engine
{
public:
	Engine();
	~Engine();

	static Engine* Get();

	// Initializes main engine loop
	void Start();
	void Shutdown();

	void SetIsFullscreen(bool IsFullscreen);
	// Resize to a different resolution, also runs `Engine->OnWindowResized`
	void ResizeWindow(int NewSizeX, int NewSizeY);
	// Handle changes to the window size. Exists in case the size changes from
	// something other than `ResizeWindow`
	void OnWindowResized(int NewSizeX, int NewSizeY);

	void LoadConfiguration();
	void BindDataModel(const ObjectHandle&);
	void Close();

	ImVec2 GetViewportInputRectSize() const;

	GameObjectManager ObjectManager;
	AllComponentManagers ComponentManagers;

	// Use `BindDataModel` when switching DataModels
	ObjectHandle DataModelRef;
	ObjectHandle WorkspaceRef;
	ObjectHandle PrimaryDataModel;

	Renderer RendererContext;
	GLFWwindow* Window = nullptr;

	EventSignal<double> OnFrameStart;
	EventSignal<double> OnFrameRenderGui;
	EventSignal<double> OnFrameEnd;

	Scene CurrentScene;

	ThreadManager ThreadManagerInstance;
	MaterialManager MaterialManagerInstance;
	TextureManager TextureManagerInstance;
	ShaderManager ShaderManagerInstance;
	MeshProvider MeshProviderInstance;
	Physics PhysicsInstance;
	History HistoryInstance;

	ShaderProgram PostFxShader;
	ShaderProgram SkyboxShader;
	ShaderProgram SeparableBlurShader;
	GpuFrameBuffer SunShadowMap;

	ImVec2 OverrideViewportDockSpacePosition = { -1.f, -1.f };
	ImVec2 OverrideViewportDockSpaceSize = { -1.f, -1.f };
	ImVec2 OverrideViewportInputSize = { 1.f, 1.f };
	ImVec2 ViewportInputPosition = { 0.f, 0.f };
	bool OverrideDefaultGuiViewportDockSpace = false;
	bool OverrideDefaultViewportInputRect = false;

	bool IsHeadlessMode = false;
	bool IsFullscreen = false;
	bool VSync = false;

	int WindowSizeX = 0;
	int WindowSizeY = 0;
	float ScaleFactor = 1.f;
	
	int FramesPerSecond = 0;
	int FpsCap = 60;

	bool DebugWireframeRendering = false;

	bool IsWindowFocused = true;

	int SystemSignal = -1;
	int ExitCode = 0;

	int argc = 0;
	char** argv = nullptr;

private:
	void m_InitializeVideo();
	void m_Render(double DeltaTime, const std::vector<EcParticleEmitter*>&);

	int m_DrawnFramesInSecond = -1;
	bool m_IsRunning = false;

	int m_WindowedWidth = 800;
	int m_WindowedHeight = 800;
	int m_WindowedPosX = 0;
	int m_WindowedPosY = 0;

	uint32_t m_FboResourceId = UINT32_MAX;
};
