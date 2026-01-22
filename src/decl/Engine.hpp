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

#include "datatype/GameObject.hpp"
#include "component/DataModel.hpp"
#include "component/Workspace.hpp"

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
	void BindDataModel(GameObject*);
	void Close();

	ImVec2 GetViewportSize() const;
	
	// Use `BindDataModel` when switching DataModels
	ObjectHandle DataModelRef;
	ObjectHandle WorkspaceRef;

	Renderer RendererContext;
	GLFWwindow* Window{};

	EventSignal<double> OnFrameStart{};
	EventSignal<double> OnFrameRenderGui{};
	EventSignal<double> OnFrameEnd{};

	Scene CurrentScene;

	ImVec2 OverrideViewportDockSpacePosition{ -1.f, -1.f };
	ImVec2 OverrideViewportDockSpaceSize{ -1.f, -1.f };
	ImVec2 OverrideViewportSize{ 1.f, 1.f };
	ImVec2 ViewportPosition{ 0.f, 0.f };
	bool OverrideDefaultGuiViewportDockSpace = false;
	bool OverrideDefaultViewport = false;

	bool IsHeadlessMode = false;
	bool IsFullscreen = false;
	bool VSync = false;

	int WindowSizeX{};
	int WindowSizeY{};
	
	int FramesPerSecond = 0;
	int FpsCap = 60;
	float PhysicsTimeScale = 1.f;

	bool DebugWireframeRendering = false;
	bool DebugAabbs = false;
	bool DebugSpatialHeat = false;

	bool IsWindowFocused = true;

	int ExitCode = 0;

	int argc = 0;
	char** argv = nullptr;

private:
	void m_InitializeVideo();
	void m_Render(double DeltaTime);

	int m_DrawnFramesInSecond = -1;
	bool m_IsRunning = false;

	int m_WindowedWidth = 800;
	int m_WindowedHeight = 800;
	int m_WindowedPosX = 0;
	int m_WindowedPosY = 0;

	ThreadManager m_ThreadManager;
	MaterialManager m_MaterialManager;
	TextureManager m_TextureManager;
	ShaderManager m_ShaderManager;
	MeshProvider m_MeshProvider;

	ShaderProgram m_PostFxShader;
	ShaderProgram m_SkyboxShader;
	ShaderProgram m_SeparableBlurShader;
	uint32_t m_SkyboxCubemap = UINT32_MAX;
	uint32_t m_DistortionTexture = UINT32_MAX;
	GpuFrameBuffer m_SunShadowMap;

	uint32_t m_FboResourceId = UINT32_MAX;
};
