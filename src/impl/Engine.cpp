#include <chrono>
#include <string>
#include <format>

#include <glad/gl.h>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_glfw.h>

#include <tinyfiledialogs.h>

#include <tracy/Tracy.hpp>

#include "Engine.hpp"

#include "component/ParticleEmitter.hpp"
#include "component/Transform.hpp"
#include "component/TreeLink.hpp"
#include "component/Camera.hpp"
#include "component/Script.hpp"
#include "component/Sound.hpp"
#include "component/Light.hpp"
#include "script/ScriptEngine.hpp"
#include "GlobalJsonConfig.hpp"
#include "ThreadManager.hpp"
#include "UserInput.hpp"
#include "Utilities.hpp"
#include "Timing.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

static Engine* EngineInstance = nullptr;

Engine* Engine::Get()
{
	assert(EngineInstance);
	return EngineInstance;
}

void Engine::ResizeWindow(int NewSizeX, int NewSizeY)
{
	ZoneScoped;

	glfwSetWindowSize(Window, NewSizeX, NewSizeY);
	this->OnWindowResized(NewSizeX, NewSizeY);
}

void Engine::OnWindowResized(int NewSizeX, int NewSizeY)
{
	ZoneScoped;

	this->WindowSizeX = NewSizeX;
	this->WindowSizeY = NewSizeY;

	RendererContext.ChangeResolution(WindowSizeX, WindowSizeY);
	Texture& fboRes = m_TextureManager.GetTextureResource(m_FboResourceId);
	fboRes.Width = NewSizeX;
	fboRes.Height = NewSizeY;
}

void Engine::SetIsFullscreen(bool MakeFullscreen)
{
	if (IsFullscreen == MakeFullscreen)
		return;

	if (MakeFullscreen)
	{
		glfwGetWindowSize(Window, &m_WindowedWidth, &m_WindowedHeight);
		// Not supported on Linux with Wayland
#ifdef _WIN32
		glfwGetWindowPos(Window, &m_WindowedPosX, &m_WindowedPosY);
#endif

		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		glfwSetWindowMonitor(Window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, GLFW_DONT_CARE);
	}
	else
	{
		glfwSetWindowMonitor(Window, nullptr, m_WindowedPosX, m_WindowedPosY, m_WindowedWidth, m_WindowedHeight, GLFW_DONT_CARE);
	}

	this->IsFullscreen = MakeFullscreen;
}

template <class T>
static T readFromConfiguration(const std::string_view& Key, const T& DefaultValue)
{
	try
	{
		return EngineJsonConfig.value(Key, DefaultValue);
	}
	catch (const nlohmann::json::type_error& Error)
	{
		Log::ErrorF(
			"Error trying to read key '{}' of configuration: {}. Falling back to default value",
			Key, Error.what()
		);

		return DefaultValue;
	}
}

void Engine::LoadConfiguration()
{
	ZoneScoped;

	bool ConfigLoadSucceeded = true;
	std::string ConfigLoadErrorMessage = "Failed to load configuration file.";
	std::string ConfigAscii = FileRW::ReadFile("./phoenix.conf", &ConfigLoadSucceeded);

	if (ConfigLoadSucceeded)
		try
		{
			nlohmann::json config = nlohmann::json::parse(ConfigAscii);
			
			for (auto it = config.begin(); it != config.end(); it++)
				if (EngineJsonConfig.find(it.key()) == EngineJsonConfig.end())
					EngineJsonConfig[it.key()] = it.value();
		}
		catch (const nlohmann::json::parse_error& err)
		{
			ConfigLoadSucceeded = false;
			
			ConfigLoadErrorMessage = std::format(
				"Failed to load configuration file: {}\nA fallback will be used.",
				err.what()
			);
		}

	if (!ConfigLoadSucceeded)
	{
		tinyfd_messageBox(
			"Configuration Error",
			ConfigLoadErrorMessage.c_str(),
			"ok",
			"warn",
			1
		);

		EngineJsonConfig =
		{
			{ "ResourcesDirectory", "resources/" }
		};
	}

	VSync = readFromConfiguration("VSync", false);
	FpsCap = readFromConfiguration("FpsCap", 60);

	std::string_view resDir = readFromConfiguration("ResourcesDirectory", std::string_view("<NOT_SET>"));

	if (resDir != "resources/")
		Log::WarningF(
			"Resources Directory was changed to '{}' instead of 'resources/'. Prepare for unforeseen consequences.",
			resDir
		);

	if (ConfigLoadSucceeded)
		Log::Info("Configuration loaded");
	else
		Log::Info(ConfigLoadErrorMessage);
}

void Engine::Close()
{
	m_IsRunning = false;
}

static void windowResizeCallback(GLFWwindow* window, int newWidth, int newHeight)
{
	Engine::Get()->OnWindowResized(newWidth, newHeight);
}

static void windowFocusChangedCallback(GLFWwindow* window, int focused)
{
	Engine::Get()->IsWindowFocused = (bool)focused;	
}

static void errorCallback(int code, const char* message)
{
	RAISE_RTF("Error occurred in GLFW:\nCode: {}, Message: {}", code, message);
}

void Engine::m_InitVideo()
{
	ZoneScoped;

	if (PHX_HEADLESS_BUILD || readFromConfiguration("Headless", false))
		this->IsHeadlessMode = true;

	if (IsHeadlessMode)
		return;

	glfwSetErrorCallback(errorCallback);

	if (!glfwInit())
	{
		const char* error = nullptr;
		int ec = glfwGetError(&error);
		RAISE_RTF("Failed to initialize GLFW: Code: {}, Error: {}", ec, error);
	}

	Log::InfoF("GLFW platform: {}", glfwGetPlatform());

	GLFWmonitor* primaryDisplay = glfwGetPrimaryMonitor();
	
	glfwGetMonitorWorkarea(primaryDisplay, nullptr, nullptr, &WindowSizeX, &WindowSizeY);

	nlohmann::json::array_t requestedGLVersion = readFromConfiguration("OpenGLVersion", nlohmann::json{ 4, 6 });
	int requestedGLVersionMajor = requestedGLVersion[0];
	int requestedGLVersionMinor = requestedGLVersion[1];

	Log::InfoF(
		"Requesting a Core OpenGL context with version {}.{}",
		requestedGLVersionMajor, requestedGLVersionMinor
	);

	// Must be set *before* window creation
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, requestedGLVersionMajor);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, requestedGLVersionMinor);

	glfwWindowHint(GLFW_DOUBLEBUFFER, 1);
	glfwWindowHint(GLFW_DEPTH_BITS, 24);
	glfwWindowHint(GLFW_STENCIL_BITS, 8);

	Window = glfwCreateWindow(
		WindowSizeX, WindowSizeY,
		readFromConfiguration<std::string_view>("GameTitle", "PhoenixEngine").data(),
		nullptr, nullptr
	);

	if (!Window)
	{
		const char* error = nullptr;
		int ec = glfwGetError(&error);
		RAISE_RTF("GLFW could not create the window\nCode: {}, Error: {}", ec, error);
	}

	Log::Info("Window created, initializing renderer...");

	this->RendererContext.Initialize(this->WindowSizeX, this->WindowSizeY, this->Window);

	Log::Info("Registering callbacks...");

	glfwSetWindowSizeCallback(Window, windowResizeCallback);
	glfwSetWindowFocusCallback(Window, windowFocusChangedCallback);

	Log::Info("Finished initializing video");
}

const int32_t SunShadowMapResolutionSq = 2048;

Engine::Engine()
{
	ZoneScopedC(tracy::Color::Aqua);

	assert(!EngineInstance);
	EngineInstance = this;
	
	this->LoadConfiguration();
	m_InitVideo();

	GameObject::s_WorldArray.reserve(32);
	FileRW::DefineAlias("cwd", std::filesystem::current_path().string());
	FileRW::DefineAlias("editres", "resources/");
	FileRW::DefineAlias("projres", "resources/");

	Log::Info("Initializing managers...");

	m_ThreadManager.Initialize(EngineJsonConfig.value("ThreadManagerThreadCount", -1));

	m_TextureManager.Initialize(IsHeadlessMode);
	m_ShaderManager.Initialize(IsHeadlessMode);
	m_MaterialManager.Initialize(); // mat after tex and shd as it may attempt to load a texture and shader
	m_MeshProvider.Initialize(IsHeadlessMode);

	if (!PHX_HEADLESS_BUILD && !IsHeadlessMode)
	{
		ZoneScopedN("Blue Frame");

		Log::Info("Blue frame...");

		RendererContext.FrameBuffer.Unbind();

		glClearColor(0.07f, 0.13f, 0.17f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		RendererContext.SwapBuffers();

		ZoneNamedN(miscshaders, "Misc shader load", true);

		m_PostFxShader = m_ShaderManager.GetShaderResource(m_ShaderManager.LoadFromPath("postprocessing"));
		m_SkyboxShader = m_ShaderManager.GetShaderResource(m_ShaderManager.LoadFromPath("skybox"));
		m_SeparableBlurShader = m_ShaderManager.GetShaderResource(m_ShaderManager.LoadFromPath("separableblur"));

		m_PostFxShader.SetUniform("Texture", 1);
		m_PostFxShader.SetUniform("DistortionTexture", 2);
		m_PostFxShader.SetUniform("BloomTexture", 3);
		m_SeparableBlurShader.SetUniform("Texture", 3);
		m_SkyboxShader.SetUniform("SkyboxCubemap", 3);

		m_DistortionTexture = m_TextureManager.LoadTextureFromPath("textures/screendistort.jpg");

		m_SunShadowMap.Initialize(
			SunShadowMapResolutionSq, SunShadowMapResolutionSq,
			/* MSSamples = */ 0,
			/* DepthOnly = */ true
		);
	}

	Log::Info("Engine initialized");
}

static bool s_DebugCollisionAabbs = false;

static void traverseHierarchy(
	Memory::vector<RenderItem, MEMCAT(Rendering)>& RenderList,
	Memory::vector<LightItem, MEMCAT(Rendering)>& LightList,
	PhysicsWorld& PhysicsWorld,
	const ObjectHandle& Root,
	EcCamera* SceneCamera,
	double DeltaTime,
	EcDirectionalLight** Sun,
	bool ScriptsUpdate = true
)
{
	ZoneScopedC(tracy::Color::LightGoldenrod);

	static uint32_t boxframeMaterial = MaterialManager::Get()->LoadFromPath("boxframe");
	static uint32_t cubeMesh = MeshProvider::Get()->LoadFromPath("!Cube");

	std::vector<GameObject*> objectsRaw = Root->GetChildren();
	// TODO 05/04/2025: FIXME
	std::vector<ObjectHandle> objects;
	objects.reserve(objectsRaw.size());

	for (GameObject* o : objectsRaw)
		objects.emplace_back(o);

	for (ObjectHandle object : objects)
	{
		if (EcSound* sound = object->FindComponent<EcSound>())
			sound->Update(DeltaTime);

		if (!object->Enabled)
			continue;
			
		EcMesh* object3D = object->FindComponent<EcMesh>();
		EcTransform* ct = object->FindComponent<EcTransform>();

		if (object3D)
		{
			if (object3D->PhysicsDynamics)
				PhysicsWorld.Dynamics.emplace_back(object);
			else if (object3D->PhysicsCollisions)
				PhysicsWorld.Statics.emplace_back(object);

			if (object3D->Transparency > .95f || Engine::Get()->IsHeadlessMode)
				continue;

			// TODO: frustum culling

			RenderList.emplace_back(
				object3D->RenderMeshId,
				ct->Transform,
				ct->Size,
				object3D->MaterialId,
				object3D->Tint,
				object3D->Transparency,
				object3D->MetallnessFactor,
				object3D->RoughnessFactor,
				object3D->FaceCulling,
				object3D->CastsShadows
			);

			if (s_DebugCollisionAabbs && object3D->PhysicsCollisions)
				RenderList.emplace_back(
					cubeMesh,
					glm::translate(glm::mat4(1.f), (glm::vec3)object3D->CollisionAabb.Position),
					object3D->CollisionAabb.Size,
					boxframeMaterial,
					glm::vec3(1.f, 1.f, 0.f),
					0.f,
					0.f,
					0.f,
					FaceCullingMode::None,
					false
				);
		}

		if (EcScript* scr = object->FindComponent<EcScript>(); ScriptsUpdate && scr)
			scr->Update(DeltaTime);

		if (EcTreeLink* link = object->FindComponent<EcTreeLink>(); link && link->Target.IsValid())
			traverseHierarchy(
				RenderList,
				LightList,
				PhysicsWorld,
				link->Target,
				SceneCamera,
				DeltaTime,
				Sun,
				link->Scripting
			);

		if (!ct)
			continue; // lights require a Transform

		EcDirectionalLight* directional = object->FindComponent<EcDirectionalLight>();
		EcPointLight* point = object->FindComponent<EcPointLight>();
		EcSpotLight* spot = object->FindComponent<EcSpotLight>();
		
		if (directional)
		{
			LightList.push_back(LightItem{
				.Position = (glm::vec3)ct->Transform[3],
				.LightColor = directional->LightColor * directional->Brightness,
				.Type = LightType::Directional,
				.Shadows = directional->Shadows
			});

			if (!*Sun && directional->Shadows)
				*Sun = directional;
		}

		if (point)
			LightList.push_back(LightItem{
				.Position = (glm::vec3)ct->Transform[3],
				.LightColor = point->LightColor * point->Brightness,
				.Range = point->Range,
				.Type = LightType::Point,
				.Shadows = false, /* point->Shadows, */
			});

		if (spot)
			LightList.push_back(LightItem{
				.Position = (glm::vec3)ct->Transform[3],
				.LightColor = spot->LightColor * spot->Brightness,
				.Range = spot->Range,
				.SpotLightDirection = (glm::vec3)ct->Transform[2],
				.Angle = spot->Angle,
				.Type = LightType::Spot,
				.Shadows = false, /* spot->Shadows, */
			});

		if (!object->GetChildren().empty())
			traverseHierarchy(
				RenderList,
				LightList,
				PhysicsWorld,
				object,
				SceneCamera,
				DeltaTime,
				Sun,
				true
			);
		
		if (EcParticleEmitter* emitter = object->FindComponent<EcParticleEmitter>())
		{
			emitter->Update(DeltaTime);
			emitter->AppendToRenderList(RenderList);
		}
	}
}

static GLuint startLoadingSkybox(std::vector<uint32_t>* skyboxFacesBeingLoaded)
{
	ZoneScoped;

	if (Engine::Get()->IsHeadlessMode)
		return 0;

	const std::string_view SkyPath = "textures/Sky1/";
	const std::string_view SkyboxCubemapImages[6] =
	{
		"right",
		"left",
		"top",
		"bottom",
		"front",
		"back"
	};

	GLuint skyboxCubemap = 0;
	glGenTextures(1, &skyboxCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	for (uint8_t faceIndex = 0; faceIndex < 6; faceIndex++)
	{
		const std::string_view& face = SkyboxCubemapImages[faceIndex];

		uint32_t tex = TextureManager::Get()->LoadTextureFromPath(std::format("{}{}.jpg", SkyPath, face));
		skyboxFacesBeingLoaded->push_back(tex);

		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
			0,
			GL_RGB,
			1,
			1,
			0,
			GL_RED,
			GL_UNSIGNED_BYTE,
			TextureManager::Get()->GetTextureResource(tex).TMP_ImageByteData
		);
	}

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	return skyboxCubemap;
}

#include "imgui_internal.h"

static ImGuiID ViewportNodeId = UINT32_MAX;

ImVec2 Engine::GetViewportSize() const
{
	return OverrideDefaultViewport ? OverrideViewportSize : ImVec2{ (float)WindowSizeX, (float)WindowSizeY };
}

void Engine::m_Render(double deltaTime)
{
	ZoneScoped;

	static const Mesh cubeMesh = m_MeshProvider.GetMeshResource(m_MeshProvider.LoadFromPath("!Cube"));
	static const Mesh quadMesh = m_MeshProvider.GetMeshResource(m_MeshProvider.LoadFromPath("!Quad"));

	if (VSync)
		glfwSwapInterval(1);
	else
		glfwSwapInterval(0);

	ImVec2 viewportSize = GetViewportSize();
	float aspectRatio = viewportSize.x / viewportSize.y;

	ObjectHandle sceneCamObject = WorkspaceRef->FindComponent<EcWorkspace>()->GetSceneCamera();
	EcCamera* sceneCamera = sceneCamObject->FindComponent<EcCamera>();

	// we do this AFTER  `traverseHierarchy` in case any Scripts
	// update the camera transform
	glm::mat4 renderMatrix = sceneCamera->GetRenderMatrix(aspectRatio);

	glm::mat4 camTrans = sceneCamera->GetWorldTransform();

	glm::mat4 view = glm::lookAt(
		glm::vec3(0.f),
		glm::vec3(camTrans[2]),
		glm::vec3(camTrans[1])
	);
	glm::mat4 projection = glm::perspective(
		glm::radians(sceneCamera->FieldOfView),
		aspectRatio,
		sceneCamera->NearPlane,
		sceneCamera->FarPlane
	);

	glm::mat4 skyRenderMatrix = projection * view;

	m_SkyboxShader.SetUniform("RenderMatrix", skyRenderMatrix);
	m_SkyboxShader.SetUniform("Time", GetRunningTime());

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_SkyboxCubemap);

	RendererContext.FrameBuffer.Bind();

	glViewport(
		0, 0,
		(int)viewportSize.x, (int)viewportSize.y
	);

	glClear(/*GL_COLOR_BUFFER_BIT |*/ GL_DEPTH_BUFFER_BIT);
	glDepthFunc(GL_LEQUAL);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	RendererContext.DrawMesh(
		cubeMesh,
		m_SkyboxShader,
		{ 1.f, 1.f, 1.f },
		skyRenderMatrix,
		FaceCullingMode::FrontFace // Cull the Outside, not the Inside
	);

	glDepthFunc(GL_LESS);

	//Main render pass
	RendererContext.DrawScene(CurrentScene, renderMatrix, sceneCamera->GetWorldTransform(), GetRunningTime(), DebugWireframeRendering);

	glViewport(0, 0, WindowSizeX, WindowSizeY);
	glDisable(GL_DEPTH_TEST);
	
	this->OnFrameRenderGui.Fire(deltaTime);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glEnable(GL_DEPTH_TEST);

	//Do framebuffer stuff after everything is drawn
	RendererContext.FrameBuffer.Unbind();

	glActiveTexture(GL_TEXTURE1);
	RendererContext.FrameBuffer.BindTexture();

	glDisable(GL_DEPTH_TEST);

	if (EngineJsonConfig.value("postfx_enabled", false))
	{
		ZoneScopedN("ApplyPostFxSettings");
	
		m_PostFxShader.SetUniform("PostFxEnabled", 1);
		m_PostFxShader.SetUniform(
			"ScreenEdgeBlurEnabled",
			EngineJsonConfig.value("postfx_blurvignette", false)
		);
		m_PostFxShader.SetUniform(
			"DistortionEnabled",
			EngineJsonConfig.value("postfx_distortion", false)
		);
	
		m_PostFxShader.SetUniform(
			"Gamma",
			EngineJsonConfig.value("postfx_gamma", 1.f)
		);
		m_SkyboxShader.SetUniform(
			"HdrEnabled",
			true
		);
	
		if (EngineJsonConfig.find("postfx_blurvignette_blurstrength") != EngineJsonConfig.end())
		{
			m_PostFxShader.SetUniform(
				"BlurVignetteStrength",
				(float)EngineJsonConfig["postfx_blurvignette_blurstrength"]
			);
			m_PostFxShader.SetUniform(
				"BlurVignetteDistMul",
				(float)EngineJsonConfig["postfx_blurvignette_weightmul"]
			);
			m_PostFxShader.SetUniform(
				"BlurVignetteDistExp",
				(float)EngineJsonConfig["postfx_blurvignette_weightexp"]
			);
			m_PostFxShader.SetUniform(
				"BlurVignetteSampleRadius",
				(int)EngineJsonConfig["postfx_blurvignette_sampleradius"]
			);
		}
	
		m_PostFxShader.SetUniform("Time", GetRunningTime());
	
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, m_TextureManager.GetTextureResource(m_DistortionTexture).GpuId);
	}
	else
	{
		m_PostFxShader.SetUniform("PostFxEnabled", 0);
		m_SkyboxShader.SetUniform(
			"HdrEnabled",
			false
		);
	}

	{
		ZoneScopedN("MainPostProcessing");
		RendererContext.DrawMesh(
			quadMesh,
			m_PostFxShader,
			{ 2.f, 2.f, 2.f },
			glm::mat4(1.f),
			FaceCullingMode::BackFace,
			0
		);
	}

	{
		ZoneScopedN("DearImGuiRender");
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	glEnable(GL_DEPTH_TEST);
}

static void ensureDataModelValid(GameObject* DataModel)
{
	PHX_ENSURE_MSG(DataModel, "DataModel is NULL!");

	GameObject* workspace = DataModel->FindChild("Workspace");
	PHX_ENSURE(workspace /* , "DataModel has no Workspace!" */);
	PHX_ENSURE(workspace->FindComponent<EcWorkspace>() /*, "Workspace masquerading!" */);
	PHX_ENSURE(DataModel->FindComponent<EcDataModel>() /*, "DataModel masquerading!" */);
}

void Engine::BindDataModel(GameObject* NewDataModel)
{
	ensureDataModelValid(NewDataModel);

	DataModelRef = NewDataModel;
	WorkspaceRef = NewDataModel->FindChild("Workspace");
	assert(WorkspaceRef.Dereference());

	GameObject::s_DataModel = DataModelRef->ObjectId;
}

void Engine::Start()
{
	Log::Info("Validating DataModel...");
	ensureDataModelValid(DataModelRef.Dereference());

	Log::Info("Final initializations...");

	ScriptEngine::Initialize(); // can only do this after datamodel is bound

	double RunningTime = GetRunningTime();

	// `LastFrameBegan` is for deltatime
	// `LastFrameEnded` is for FPS cap
	double LastFrameBegan = RunningTime;
	double LastFrameEnded = RunningTime;
	double LastSecond = 0.f;

	std::vector<uint32_t> skyboxFacesBeingLoaded;
	skyboxFacesBeingLoaded.reserve(6);

	m_SkyboxCubemap = startLoadingSkybox(&skyboxFacesBeingLoaded);

	m_FboResourceId = m_TextureManager.Assign({
		.ImagePath = "!Framebuffer:Main",
		.ResourceId = UINT32_MAX,
		.GpuId = RendererContext.FrameBuffer.GpuTextureId,
		.Width = WindowSizeX, .Height = WindowSizeY,
		.NumColorChannels = 3
	}, "!Framebuffer:Main");

	CurrentScene.RenderList.reserve(50);

	PhysicsWorld physWorld;

	m_IsRunning = true;

	Log::Info("Main engine loop start");

	while ((!PHX_HEADLESS_BUILD && !IsHeadlessMode) ? (!glfwWindowShouldClose(Window) && m_IsRunning) : m_IsRunning)
	{
		TIME_SCOPE_AS_N("EntireFrame", EntireFrameTimerScope);
		ZoneScopedNC("Frame", tracy::Color::PaleTurquoise);

		if (DataModelRef->IsDestructionPending)
		{
			Log::Warning("`::Destroy` called on DataModel, shutting down");
			break;
		}

		if (WorkspaceRef->IsDestructionPending)
		{
			Log::Warning("`::Destroy` called on Workspace, shutting down");
			break;
		}

		RunningTime = GetRunningTime();
		RendererContext.AccumulatedDrawCallCount = 0;

		this->FpsCap = std::clamp(this->FpsCap, 1, INT32_MAX);
		int throttledFpsCap = IsWindowFocused ? FpsCap : 10;

		double deltaTime = RunningTime - LastFrameEnded;
		double fpsCapDelta = 1.f / throttledFpsCap;
		double lastFrameTime = Timing::FinalFrameTimes[EntireFrameTimerScope.TimerId];

		// make sure the order of timers is
		// 0 - EntireFrame, 1 - FrameSleep, 2 - FrameWork
		// `drawDeveloperUI` depends on this
		static Timing::StaticMagicTimerThing FrameSleepTimerScope("FrameSleep");

		// Wait the appropriate amount of time between frames
		if ((!VSync || !IsWindowFocused) && (deltaTime < fpsCapDelta))
		{
			Timing::ScopedTimer scoped(FrameSleepTimerScope.TimerId);
			ZoneScopedNC("SleepForFpsCap", tracy::Color::Wheat);

			std::this_thread::sleep_for(std::chrono::duration<double>(fpsCapDelta - deltaTime - lastFrameTime));
		}

		TIME_SCOPE_AS("FrameWork");

		if (!IsHeadlessMode)
			m_TextureManager.FinalizeAsyncLoadedTextures();
		
		m_MeshProvider.FinalizeAsyncLoadedMeshes();

		if (skyboxFacesBeingLoaded.size() == 6 && !IsHeadlessMode)
		{
			bool skyboxLoaded = true;

			for (uint32_t skyboxFace : skyboxFacesBeingLoaded)
			{
				Texture& texture = m_TextureManager.GetTextureResource(skyboxFace);

				if (texture.Status != Texture::LoadStatus::Succeeded)
				{
					skyboxLoaded = false;
					break;
				}
			}

			if (skyboxLoaded)
			{
				ZoneScopedN("UploadSkyboxToGpu");
				
				glBindTexture(GL_TEXTURE_CUBE_MAP, m_SkyboxCubemap);

				for (int skyboxFaceIndex = 0; skyboxFaceIndex < 6; skyboxFaceIndex++)
				{
					Texture& texture = m_TextureManager.GetTextureResource(skyboxFacesBeingLoaded.at(skyboxFaceIndex));

					glTexImage2D(
						GL_TEXTURE_CUBE_MAP_POSITIVE_X + skyboxFaceIndex,
						0,
						GL_RGB,
						texture.Width,
						texture.Height,
						0,
						texture.NumColorChannels == 1 ? GL_RED
							: texture.NumColorChannels == 2 ? GL_RG
							: texture.NumColorChannels == 3 ? GL_RGB
							: GL_RGBA,
						GL_UNSIGNED_BYTE,
						texture.TMP_ImageByteData
					);

					//glGenerateMipmap(GL_TEXTURE_2D);

					free(texture.TMP_ImageByteData);
					texture.TMP_ImageByteData = nullptr;
				}

				glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

				skyboxFacesBeingLoaded.clear();
			}
		}

		RunningTime = GetRunningTime();
		deltaTime = RunningTime - LastFrameBegan;

		LastFrameBegan = RunningTime;

		this->OnFrameStart.Fire(deltaTime);

		{
			ZoneScopedNC("PollEvents", tracy::Color::Orange);

			glfwPollEvents();
		}

		if (!IsHeadlessMode)
		{
			ZoneNamedN(imguizone, "NewDearImGuiFrame", true);
			// so scripts can use the `imgui` library
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			if (!OverrideDefaultGuiViewportDockSpace)
				ViewportNodeId = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
			else
			{
				ImGuiViewport* viewport = ImGui::GetMainViewport();
				viewport->WorkPos = OverrideViewportDockSpacePosition;
				viewport->WorkSize = OverrideViewportDockSpaceSize;

				ViewportNodeId = ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
			}
		}

		ScriptEngine::StepScheduler(); // scripts may try to draw Dear ImGui, this needs to be AFTER `ImGui::NewFrame`

		EcWorkspace* workspaceComponent = WorkspaceRef->FindComponent<EcWorkspace>();
		ObjectHandle sceneCamObject = workspaceComponent->GetSceneCamera();
		EcCamera* sceneCamera = sceneCamObject->FindComponent<EcCamera>();

		if (!IsHeadlessMode)
			workspaceComponent->Update();

		REFLECTION_SIGNAL_EVENT(DataModelRef->FindComponent<EcDataModel>()->OnFrameBeginCallbacks, deltaTime);

		// fetch the camera again because of potential CurrentScene changes that may have caused re-alloc'd
		// (really need a generic `Ref` system)
		sceneCamera = sceneCamObject->FindComponent<EcCamera>();

		s_DebugCollisionAabbs = this->DebugAabbs;
		EcDirectionalLight* sun = nullptr;
		{
			TIME_SCOPE_AS("TraverseHierarchy");

			CurrentScene.RenderList.clear();
			CurrentScene.LightingList.clear();
			physWorld.Dynamics.clear();
			physWorld.Statics.clear();

			// Aggregate mesh and light data into lists
			traverseHierarchy(
				CurrentScene.RenderList,
				CurrentScene.LightingList,
				physWorld,
				WorkspaceRef,
				sceneCamera,
				deltaTime,
				&sun
			);

			if (DebugSpatialHeat)
			{
				workspaceComponent = WorkspaceRef->FindComponent<EcWorkspace>();
				assert(workspaceComponent);

				for (const auto& it : workspaceComponent->SpatialHash)
				{
					CurrentScene.RenderList.push_back(RenderItem{
						.RenderMeshId = 0,
						.Transform = glm::translate(glm::mat4(1.f), (glm::vec3)it.first),
						.Size = glm::vec3(SPATIAL_HASH_GRID_SIZE),
						.MaterialId = m_MaterialManager.LoadFromPath("neon"),
						.TintColor = glm::vec3(1.f, 0.f, 0.f),
						.Transparency = std::clamp(1.f - ((float)it.second.size() / 64.f), 0.2f, 1.f),
						.FaceCulling = FaceCullingMode::None
					});
				}
			}

			sceneCamera = sceneCamObject->FindComponent<EcCamera>();
		}

		if (physWorld.Dynamics.size() > 0)
			Physics::Step(physWorld, deltaTime * PhysicsTimeScale);

		if (!IsHeadlessMode)
		{
			CurrentScene.UsedShaders.clear();

			for (const RenderItem& ri : CurrentScene.RenderList)
				CurrentScene.UsedShaders.insert(m_MaterialManager.GetMaterialResource(ri.MaterialId).ShaderId);
		}

		if (!IsHeadlessMode && sun)
		{
			TIME_SCOPE_AS("Shadows");
			ZoneScopedN("Shadows");

			Scene sunScene{};
			sunScene.RenderList.reserve(CurrentScene.RenderList.size());
			sunScene.UsedShaders = CurrentScene.UsedShaders;

			for (const RenderItem& ri : CurrentScene.RenderList)
				if (ri.CastsShadows)
				{
					sunScene.RenderList.push_back(ri);
					sunScene.RenderList.back().FaceCulling = FaceCullingMode::FrontFace;
				}

			glm::vec3 sunDirection = glm::vec3(sun->Object->FindComponent<EcTransform>()->Transform[3]);
			
			glm::mat4 sunOrtho = glm::ortho(
				-sun->ShadowViewSizeH, sun->ShadowViewSizeH, -sun->ShadowViewSizeV, sun->ShadowViewSizeV,
				sun->ShadowViewNearPlane, sun->ShadowViewFarPlane
			);
			glm::mat4 sunView = glm::lookAt(
				glm::normalize(sunDirection) * sun->ShadowViewDistance,
				glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f)
			);
			sunView[3] = glm::vec4(glm::vec3(sunView[3]) + sun->ShadowViewOffset, 1.f);
			if (sun->ShadowViewMoveWithCamera)
				sunView[3] = glm::vec4(glm::vec3(sunView[3]) - glm::vec3(sceneCamera->GetWorldTransform()[3]), 1.f);

			glm::mat4 sunRenderMatrix = sunOrtho * sunView;

			m_SunShadowMap.Bind();
			glViewport(0, 0, SunShadowMapResolutionSq, SunShadowMapResolutionSq);
			glClear(/*GL_COLOR_BUFFER_BIT |*/ GL_DEPTH_BUFFER_BIT);

			for (uint32_t shdId : CurrentScene.UsedShaders)
			{
				ShaderProgram& shd = m_ShaderManager.GetShaderResource(shdId);
				shd.SetUniform("IsShadowMap", true);

				glActiveTexture(GL_TEXTURE0 + 101);
				m_SunShadowMap.BindTexture();
				shd.SetUniform("ShadowAtlas", 101);

				shd.SetUniform("DirecLightProjection", sunRenderMatrix);
			}

			RendererContext.DrawScene(sunScene, sunRenderMatrix, glm::mat4(1.f), RunningTime, DebugWireframeRendering);

			for (uint32_t shdId : CurrentScene.UsedShaders)
				m_ShaderManager.GetShaderResource(shdId).SetUniform("IsShadowMap", false);

			glViewport(0, 0, WindowSizeX, WindowSizeY);
		}

		if (!IsHeadlessMode)
		{
			m_Render(deltaTime);
			RendererContext.SwapBuffers();
		}

		// End of frame
		RunningTime = GetRunningTime();

		LastFrameEnded = RunningTime;

		m_DrawnFramesInSecond++;

		OnFrameEnd.Fire(deltaTime);

		if (RunningTime - LastSecond > 1.f)
		{
			LastSecond = RunningTime;

			this->FramesPerSecond = m_DrawnFramesInSecond;
			m_DrawnFramesInSecond = -1;

			Log::Save();
		}

		Memory::FrameFinish();
		Timing::Finish();
		FrameMark;
	}

	Log::Info("Main loop exited");
}

void Engine::Shutdown()
{
	ZoneScoped;

	Log::Info("Engine destructing...");

	Log::Save();

#ifdef NDEBUG
	Log::Info("Clearing World Array...");
	GameObject::s_WorldArray.clear();
#else
	// Helps ensure correctness, but slower
	Log::Info("Destroying DataModel...");
	DataModelRef->Destroy();
	WorkspaceRef->Destroy();
	DataModelRef.Clear();
	WorkspaceRef.Clear();
#endif

	Log::Info("Shutting down Component Managers...");

	// skip the first "None" component manager
	for (size_t i = 1; i < (size_t)EntityComponent::__count; i++)
	{
		ZoneScopedN("Shutdown Component");
		ZoneText(s_EntityComponentNames[i].data(), s_EntityComponentNames[i].size());

		GameObject::s_ComponentManagers[i]->Shutdown();
	}

	Log::Info("Shutting down managers...");

	m_MaterialManager.Shutdown();
	m_TextureManager.Shutdown();
	m_ShaderManager.Shutdown();
	m_MeshProvider.Shutdown();

	m_ThreadManager.Shutdown();

	Log::Info("Shutting down libraries...");

	if (!IsHeadlessMode)
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
	}
	
	if (Window)
		glfwDestroyWindow(Window);
	glfwTerminate();

	EngineInstance = nullptr;
}

Engine::~Engine()
{
	assert(!EngineInstance);
}
