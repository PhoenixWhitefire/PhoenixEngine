#include <chrono>
#include <string>
#include <format>

#include <glad/include/glad/gl.h>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_glfw.h>

#include <tinyfiledialogs.h>

#include <tracy/Tracy.hpp>

#include "Engine.hpp"

#include "component/ParticleEmitter.hpp"
#include "component/InputService.hpp"
#include "component/Environment.hpp"
#include "component/Transform.hpp"
#include "component/RigidBody.hpp"
#include "component/Interface.hpp"
#include "component/Animation.hpp"
#include "component/TreeLink.hpp"
#include "component/Camera.hpp"
#include "component/Sound.hpp"
#include "component/Light.hpp"
#include "script/ScriptEngine.hpp"
#include "render/TextureSlots.hpp"
#include "GlobalJsonConfig.hpp"
#include "UserInput.hpp"
#include "Utilities.hpp"
#include "Timing.hpp"
#include "FileRW.hpp"
#include "Log.hpp"
#include "Stl.hpp"

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
    if (NewSizeX == 0 || NewSizeY == 0)
        return;

    this->WindowSizeX = NewSizeX;
    this->WindowSizeY = NewSizeY;

    RendererContext.ChangeResolution(WindowSizeX, WindowSizeY);
    Texture& fboRes = TextureManagerInstance.GetTextureResource(m_FboResourceId);
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
		Log.ErrorF(
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
	std::string ConfigLoadErrorMessage = "Failed to load configuration file. Please ensure the Engine was launched from the correct directory.";
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

		EngineJsonConfig = {
			{ "ResourcesDirectory", "resources/" }
		};
	}

	VSync = readFromConfiguration("VSync", false);
	FpsCap = readFromConfiguration("FpsCap", 60);
	ScriptEngine::DefaultVMAllowedExecutionTime = readFromConfiguration("DefaultVMAllowedExecutionTime", 10.0);

	std::string_view resDir = readFromConfiguration("ResourcesDirectory", std::string_view("<NOT_SET>"));

	if (resDir != "resources/")
		Log.WarningF(
			"Resources Directory was changed to '{}' instead of 'resources/'. Prepare for unforeseen consequences.",
			resDir
		);

	if (ConfigLoadSucceeded)
		Log.Info("Configuration loaded");
	else
		Log.Info(ConfigLoadErrorMessage);
}

void Engine::Close()
{
	m_IsRunning = false;
}

static void windowResizeCallback(GLFWwindow*, int newWidth, int newHeight)
{
	Engine::Get()->OnWindowResized(newWidth, newHeight);
}

static void windowFocusChangedCallback(GLFWwindow*, int focused)
{
	Engine::Get()->IsWindowFocused = (bool)focused;	
}

static void windowKeyCallback(GLFWwindow*, int key, int scancode, int action, int mods)
{
	REFLECTION_SIGNAL_EVENT(EcPlayerInput::KeyEventCallbacks, InputEvent{
		.Key = {
			.Button = key,
			.Scancode = scancode,
			.Action = action,
			.Modifiers = mods
		},
		.Type = InputEventType::Key
	});
}

static void windowMouseCallback(GLFWwindow*, int button, int action, int mods)
{
	REFLECTION_SIGNAL_EVENT(EcPlayerInput::MouseButtonEventCallbacks, InputEvent{
		.MouseButton = {
			.Button = button,
			.Action = action,
			.Modifiers = mods
		},
		.Type = InputEventType::MouseButton
	});
}

static void windowScrollCallback(GLFWwindow*, double xoffset, double yoffset)
{
	REFLECTION_SIGNAL_EVENT(EcPlayerInput::ScrollEventCallbacks, InputEvent{
		.Scroll = {
			.XOffset = xoffset,
			.YOffset = yoffset
		},
		.Type = InputEventType::Scroll
	});
}

static void errorCallback(int code, const char* message)
{
	Log.ErrorF("Error occurred in GLFW:\nCode: {}, Message: {}", code, message);
}

static void windowContentScaleCallback(GLFWwindow*, float x, float)
{
	static bool loadedVectorFont = false;
	const ImGuiStyle& currentStyle = ImGui::GetStyle();
	ImGuiIO& io = ImGui::GetIO();

	ImGuiStyle newStyle;
	memcpy(newStyle.Colors, currentStyle.Colors, sizeof(ImVec4) * ImGuiCol_COUNT);
	newStyle.ScaleAllSizes(x);

	if (x != 1.f)
	{
		if (!loadedVectorFont)
			io.Fonts->AddFontDefaultVector();
		loadedVectorFont = true;
	}
	else
	{
		if (loadedVectorFont)
			io.Fonts->AddFontDefaultBitmap();
		loadedVectorFont = false;
	}

	ImGui::GetStyle() = newStyle;

	Engine::Get()->ScaleFactor = x;
}

static void updateImGuiForDisplayScaling()
{
	GLFWwindow* window = glfwGetCurrentContext();

	float xScale = 0.f;
	float yScale = 0.f;
	glfwGetWindowContentScale(window, &xScale, &yScale);
	windowContentScaleCallback(window, xScale, yScale);
}

void Engine::m_InitializeVideo()
{
#if !PHX_HEADLESS_BUILD

	ZoneScoped;
	if (IsHeadlessMode)
		return;

	glfwSetErrorCallback(errorCallback);

	GLFWallocator allocator = {
		.allocate = [](size_t size, void*)
			{
				assert(size < UINT32_MAX);
				return Memory::Alloc((uint32_t)size, MEMCAT(Glfw));
			},
		.reallocate = [](void* ptr, size_t size, void*)
			{
				assert(size < UINT32_MAX);
				return Memory::ReAlloc(ptr, (uint32_t)size, MEMCAT(Glfw));
			},
		.deallocate = [](void* ptr, void*)
			{
				Memory::Free(ptr);
			}
	};
	glfwInitAllocator(&allocator);

	if (!glfwInit())
	{
		const char* error = nullptr;
		int ec = glfwGetError(&error);
		RAISE_RT("Failed to initialize GLFW: Code: {}, Error: {}", ec, error);
	}

	Log.InfoF("GLFW platform: {}", glfwGetPlatform());

	GLFWmonitor* primaryDisplay = glfwGetPrimaryMonitor();
	
	glfwGetMonitorWorkarea(primaryDisplay, nullptr, nullptr, &WindowSizeX, &WindowSizeY);

	nlohmann::json::array_t requestedGLVersion = readFromConfiguration("OpenGLVersion", nlohmann::json{ 4, 6 });
	int requestedGLVersionMajor = requestedGLVersion[0];
	int requestedGLVersionMinor = requestedGLVersion[1];

	Log.InfoF(
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
	glfwWindowHint(GLFW_SRGB_CAPABLE, 1);

	Window = glfwCreateWindow(
		WindowSizeX, WindowSizeY,
		readFromConfiguration<std::string_view>("GameTitle", "PhoenixEngine").data(),
		nullptr, nullptr
	);

	if (!Window)
	{
		const char* error = nullptr;
		int ec = glfwGetError(&error);
		RAISE_RT("GLFW could not create the window\nCode: {}, Error: {}", ec, error);
	}

	Log.Info("Window created, initializing renderer...");

	this->RendererContext.Initialize(this->WindowSizeX, this->WindowSizeY, this->Window);

	Log.Info("Registering callbacks...");

	glfwSetWindowSizeCallback(Window, windowResizeCallback);
	glfwSetWindowFocusCallback(Window, windowFocusChangedCallback);
	glfwSetKeyCallback(Window, windowKeyCallback);
	glfwSetMouseButtonCallback(Window, windowMouseCallback);
	glfwSetScrollCallback(Window, windowScrollCallback);
	glfwSetWindowContentScaleCallback(Window, windowContentScaleCallback);

	Log.Info("Finished initializing video");

#else
	Log.Info("Skipping video initialization (headless build)");
#endif
}

const int32_t SunShadowMapResolutionSq = 2048;

std::string GetUserHomeDirectoryPath();

std::string GetUserHomeDirectoryPath()
{
	std::string home;

#ifdef _WIN32
	const char* homeCstr = std::getenv("USERPROFILE");

	if (!homeCstr)
	{
		const char* drive = std::getenv("HOMEDRIVE");
		const char* path = std::getenv("HOMEPATH");
		if (drive && path)
			home = std::string(drive) + "/" + std::string(path);
		else
			home = std::filesystem::current_path().string();
	}
	else
	{
		home = homeCstr;
	}
#else
	if (const char* homePath = std::getenv("HOME"))
		home = homePath;
	else
		home = std::filesystem::current_path().string();
#endif

	return home;
}

Engine::Engine()
{
    ZoneScopedC(tracy::Color::Aqua);

    assert(!EngineInstance);
    EngineInstance = this;

    this->LoadConfiguration();

	if (PHX_HEADLESS_BUILD || readFromConfiguration("Headless", false))
		this->IsHeadlessMode = true;

    m_InitializeVideo();

    FileRW::DefineAlias("cwd", std::filesystem::current_path().string());
    FileRW::DefineAlias("home", GetUserHomeDirectoryPath());
    FileRW::DefineAlias("base", "resources");
    FileRW::DefineAlias("project", ".");

    Log.Info("Initializing managers...");

    ThreadManagerInstance.Initialize(EngineJsonConfig.value("ThreadManagerThreadCount", -1));

    TextureManagerInstance.Initialize(IsHeadlessMode);
    ShaderManagerInstance.Initialize(IsHeadlessMode);
    MaterialManagerInstance.Initialize(); // mat after tex and shd as it may attempt to load a texture and shader
    MeshProviderInstance.Initialize(IsHeadlessMode);

    if (!PHX_HEADLESS_BUILD && !IsHeadlessMode)
    {
        {
            ZoneScopedN("Blue Frame");
            Log.Info("Blue frame...");

            RendererContext.FrameBuffer.Unbind();

            glDisable(GL_FRAMEBUFFER_SRGB);

            glClearColor(0.07f, 0.13f, 0.17f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            RendererContext.SwapBuffers();
            glfwMaximizeWindow(Window); // Can only maximize after Texture Manager is initialized

            glDisable(GL_FRAMEBUFFER_SRGB);
        }

        ZoneScopedN("Load core shaders and sun shadowmap");

        PostFxShader = ShaderManagerInstance.GetShaderResource(ShaderManagerInstance.LoadFromPath("@base/shaders/postprocessing.shp"));
        SkyboxShader = ShaderManagerInstance.GetShaderResource(ShaderManagerInstance.LoadFromPath("@base/shaders/skybox.shp"));

        PostFxShader.SetUniform("Phoenix_FramebufferTexture", ReservedTextureSlot::PostProcessFramebuffer);
        SkyboxShader.SetUniform("Phoenix_SkyboxEquirectangular", ReservedTextureSlot::SkyboxEquirectangular);
        SkyboxShader.SetUniform("Phoenix_SkyboxCubemap", ReservedTextureSlot::SkyboxCubemap);
        //PostFxShader.SetUniform("Phoenix_BloomTexture", 3);

        SunShadowMap.Initialize(
            SunShadowMapResolutionSq, SunShadowMapResolutionSq,
            /* MSSamples = */ 0,
            /* DepthOnly = */ true
        );
	}

	Log.Info("Engine initialized");
}

static void traverseHierarchy(
	Scene& RendererScene,
	Physics::World& PhysicsWorld,
	std::vector<EcParticleEmitter*>& ParticleEmitters,
	GameObject* Root,
	EcCamera* SceneCamera,
	double DeltaTime,
	EcDirectionalLight** Sun,
	bool DebugCollisionAabbs
)
{
	ZoneScopedC(tracy::Color::LightGoldenrod);

	static uint32_t boxframeMaterial = UINT32_MAX;
	static uint32_t cubeMesh = MeshProvider::Get()->LoadFromPath("!Cube");

	Root->ForEachChild([&](const ObjectHandle& object) -> bool
	{
		if (EcSound* sound = object->FindComponent<EcSound>(); sound && !Engine::Get()->IsHeadlessMode)
			sound->Update(DeltaTime);

		if (!object->Enabled)
			return true; // continue

		EcTransform* ct = object->FindComponent<EcTransform>();
		// both useless without a Transform
		EcMesh* cm = ct ? object->FindComponent<EcMesh>() : nullptr;
		EcRigidBody* rb = ct ? object->FindComponent<EcRigidBody>() : nullptr;

		if (rb)
		{
			if (rb->PhysicsDynamics)
				PhysicsWorld.Dynamics.emplace_back(object);
			else if (rb->PhysicsCollisions)
				PhysicsWorld.Statics.emplace_back(object);

			if (DebugCollisionAabbs && rb->PhysicsCollisions)
			{
				if (boxframeMaterial == UINT32_MAX)
					boxframeMaterial = MaterialManager::Get()->LoadFromPath("@base/materials/boxframe.mtl");

				RendererScene.RenderList.emplace_back(
					cubeMesh,
					glm::translate(glm::mat4(1.f), rb->CollisionAabb.Position),
					rb->CollisionAabb.Size,
					boxframeMaterial,
					glm::vec3(1.f, 1.f, 0.f),
					0.f,
					0.f,
					0.f,
					FaceCullingMode::None,
					false
				);
			}
		}

		if (cm)
		{
			if (cm->Transparency > .95f || Engine::Get()->IsHeadlessMode)
				return true; // continue

			// TODO: frustum culling

			RendererScene.RenderList.emplace_back(
				cm->RenderMeshId,
				ct->Transform,
				ct->Size,
				cm->MaterialId,
				cm->Tint,
				cm->Transparency,
				cm->MetalnessFactor,
				cm->RoughnessFactor,
				cm->FaceCulling,
				cm->CastsShadows
			);
		}

		if (EcTreeLink* link = object->FindComponent<EcTreeLink>(); link && link->Target.IsValid())
			traverseHierarchy(
				RendererScene,
				PhysicsWorld,
				ParticleEmitters,
				link->Target,
				SceneCamera,
				DeltaTime,
				Sun,
				DebugCollisionAabbs
			);

		EcDirectionalLight* directional = object->FindComponent<EcDirectionalLight>();
		EcPointLight* point = object->FindComponent<EcPointLight>();
		EcSpotLight* spot = object->FindComponent<EcSpotLight>();
		
		if (directional)
		{
			RendererScene.LightingList.push_back(LightItem{
				.Position = directional->Direction,
				.LightColor = directional->LightColor * directional->Brightness,
				.Type = LightType::Directional,
				.Shadows = directional->Shadows
			});

			if (!*Sun && directional->Shadows)
				*Sun = directional;
		}

		if (ct)
		{
			if (point)
				RendererScene.LightingList.push_back(LightItem{
					.Position = (glm::vec3)ct->Transform[3],
					.LightColor = point->LightColor * point->Brightness,
					.Range = point->Range,
					.Type = LightType::Point,
					.Shadows = false, /* point->Shadows, */
				});

			if (spot)
				RendererScene.LightingList.push_back(LightItem{
					.Position = (glm::vec3)ct->Transform[3],
					.LightColor = spot->LightColor * spot->Brightness,
					.Range = spot->Range,
					.SpotLightDirection = (glm::vec3)ct->Transform[2],
					.Angle = spot->Angle,
					.Type = LightType::Spot,
					.Shadows = false, /* spot->Shadows, */
				});
		}

		if (EcAnimator* animator = object->FindComponent<EcAnimator>())
			animator->Step(DeltaTime);

		if (!object->Children.empty())
			traverseHierarchy(
				RendererScene,
				PhysicsWorld,
				ParticleEmitters,
				object.Dereference(),
				SceneCamera,
				DeltaTime,
				Sun,
				DebugCollisionAabbs
			);
		
		if (EcParticleEmitter* emitter = object->FindComponent<EcParticleEmitter>())
		{
			emitter->Update(DeltaTime);
			ParticleEmitters.push_back(emitter);
		}

		return true; // continue
	});
}

static void traverseAndRenderUIHierarchy(
	GameObject* Root,
	Renderer& renderer,
	ShaderProgram& shader,
	const MeshProvider::GpuMesh& gpuMesh,
	const glm::vec2 Position = glm::vec3(0.f, 0.f, 0.f),
	const glm::vec2 Size = glm::vec3(1.f, 1.f, 1.f),
	const float Rotation = 0.f
)
{
	ZoneScoped;

	Root->ForEachChild([&](const ObjectHandle& child) -> bool
	{
		if (!child->Enabled)
			return true;

		EcTreeLink* et = child->FindComponent<EcTreeLink>();
		if (GameObject* targetInterface = (et ? et->Target.Referred() : nullptr); targetInterface && targetInterface->OwningDataModel != PHX_GAMEOBJECT_NULL_ID)
		{
			traverseAndRenderUIHierarchy(targetInterface, renderer, shader, gpuMesh, Position, Size, Rotation);

			return true; // continue
		}

		glm::vec2 currentPosition = Position;
		glm::vec2 currentSize = Size;
		float currentRotation = Rotation;

		EcUITransform* uit = child->FindComponent<EcUITransform>();

		if (uit)
		{
			currentPosition += uit->Position * currentSize;
			currentSize *= uit->Size;
			currentRotation += uit->Rotation;
		}

		shader.SetUniform("Phoenix_Position", currentPosition);
		shader.SetUniform("Phoenix_Size", currentSize);
		shader.SetUniform("Phoenix_Rotation", currentRotation);

		if (EcUIFrame* uf = child->FindComponent<EcUIFrame>())
		{
			shader.SetUniform("Phoenix_BackgroundColor", uf->BackgroundColor.ToGenericValue());
			shader.SetUniform("Phoenix_BackgroundTransparency", uf->BackgroundTransparency);
			shader.SetUniform("Phoenix_IsImage", false);
			shader.Activate();

			glDrawElements(GL_TRIANGLES, gpuMesh.NumIndices, GL_UNSIGNED_INT, 0);
			renderer.AccumulatedDrawCallCount++;
		}

		if (EcUIImage* uimg = child->FindComponent<EcUIImage>())
		{
			TextureManager* TextureManager = TextureManager::Get();

			shader.SetUniform("Phoenix_BackgroundColor", uimg->ImageTint.ToGenericValue());
			shader.SetUniform("Phoenix_BackgroundTransparency", uimg->ImageTransparency);
			shader.SetUniform("Phoenix_IsImage", true);
			shader.SetTextureUniform("Phoenix_Image", TextureManager->LoadFromPath(uimg->Image));
			shader.Activate();

			glDrawElements(GL_TRIANGLES, gpuMesh.NumIndices, GL_UNSIGNED_INT, 0);
			renderer.AccumulatedDrawCallCount++;
		}

		traverseAndRenderUIHierarchy(child.Dereference(), renderer, shader, gpuMesh, currentPosition, currentSize, currentRotation);
		return true; // continue
	});
}

static void renderUIElements(GameObject* Root, Renderer& renderer)
{
	ZoneScoped;
	glDisable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);

	static MeshProvider* MeshProvider = MeshProvider::Get();
	static ShaderManager* shdManager = ShaderManager::Get();
	static uint32_t quadMeshId = MeshProvider->LoadFromPath("!Quad");

	const Mesh& quadMesh = MeshProvider->GetMeshResource(quadMeshId);
	const MeshProvider::GpuMesh& gpuMesh = MeshProvider->GetGpuMesh(quadMesh.GpuId);
	gpuMesh.VertexArray.Bind();
	gpuMesh.VertexBuffer.Bind();
	gpuMesh.ElementBuffer.Bind();

	static const uint32_t shaderId = shdManager->LoadFromPath("@base/shaders/ui.shp");
	ShaderProgram& shader = shdManager->GetShaderResource(shaderId);

	traverseAndRenderUIHierarchy(Root, renderer, shader, gpuMesh);
}

ImVec2 Engine::GetViewportInputRectSize() const
{
	return OverrideDefaultViewportInputRect ? OverrideViewportInputSize : ImVec2((float)WindowSizeX, (float)WindowSizeY);
}

void Engine::m_Render(double deltaTime, const std::vector<EcParticleEmitter*>& particleEmitters)
{
    ZoneScoped;

    static const Mesh cubeMesh = MeshProviderInstance.GetMeshResource(MeshProviderInstance.LoadFromPath("!Cube"));
    static const Mesh quadMesh = MeshProviderInstance.GetMeshResource(MeshProviderInstance.LoadFromPath("!Quad"));

    if (VSync)
        glfwSwapInterval(1);
    else
        glfwSwapInterval(0);

    ImVec2 viewportSize = GetViewportInputRectSize();
    float aspectRatio = viewportSize.x / viewportSize.y;

    const ObjectHandle& sceneCamObject = WorkspaceRef->FindComponent<EcWorkspace>()->GetSceneCamera();
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

    SkyboxShader.SetUniform("Phoenix_RenderMatrix", skyRenderMatrix);
    SkyboxShader.SetUniform("Phoenix_Time", GetRunningTime());

    const EcEnvironmentService* env = ComponentManagers.Environment.GetService();

    if (env->SkyboxIsEquirectangularImage)
    {
        glActiveTexture(GL_TEXTURE0 + ReservedTextureSlot::SkyboxEquirectangular);
        glBindTexture(GL_TEXTURE_2D, env->SkyboxTextureGpuId);
        SkyboxShader.SetUniform("Phoenix_IsSkyboxEquirectangular", true);
    }
    else
    {
        glActiveTexture(GL_TEXTURE0 + ReservedTextureSlot::SkyboxCubemap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, env->SkyboxTextureGpuId);
        SkyboxShader.SetUniform("Phoenix_IsSkyboxEquirectangular", false);
    }

	RendererContext.FrameBuffer.Bind();

	glViewport(
		0, 0,
		(int)viewportSize.x, (int)viewportSize.y
	);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	RendererContext.DrawMesh(
		cubeMesh,
		SkyboxShader,
		{ 1.f, 1.f, 1.f },
		skyRenderMatrix,
		FaceCullingMode::FrontFace // Cull the Outside, not the Inside
	);

	glEnable(GL_DEPTH_TEST);

	// Main render pass
	RendererContext.DrawScene(CurrentScene, renderMatrix, sceneCamera->GetWorldTransform(), GetRunningTime(), DebugWireframeRendering);

	for (EcParticleEmitter* emitter : particleEmitters)
		emitter->Render(renderMatrix);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	if (GameObject* interface = DataModelRef->FindChildWithComponent(EntityComponent::Interface))
		renderUIElements(interface, RendererContext);

	//Do framebuffer stuff after everything is drawn

	glActiveTexture(GL_TEXTURE0 + ReservedTextureSlot::PostProcessFramebuffer);
	RendererContext.FrameBuffer.BindTexture();

	if (env->PostProcess)
	{
		ZoneScopedN("ApplyPostFxSettings");

		PostFxShader.SetUniform("Phoenix_PostFxEnabled", 1);
		PostFxShader.SetUniform(
			"Phoenix_ScreenEdgeBlurEnabled",
			EngineJsonConfig.value("postfx_blurvignette", false)
		);
		PostFxShader.SetUniform(
			"Phoenix_DistortionEnabled",
			EngineJsonConfig.value("postfx_distortion", false)
		);

		PostFxShader.SetUniform(
			"Phoenix_Gamma",
			env->GammaCorrection
		);
		SkyboxShader.SetUniform(
			"Phoenix_HdrEnabled",
			true
		);

		PostFxShader.SetUniform("Phoenix_Time", GetRunningTime());
	}
	else
	{
		PostFxShader.SetUniform("Phoenix_PostFxEnabled", false);
		SkyboxShader.SetUniform(
			"Phoenix_HdrEnabled",
			false
		);
	}

	glViewport(0, 0, WindowSizeX, WindowSizeY);
	glDisable(GL_FRAMEBUFFER_SRGB);

	{
		ZoneScopedN("MainPostProcessing");

		RendererContext.DrawMesh(
			quadMesh,
			PostFxShader,
			{ 2.f, 2.f, 2.f },
			glm::mat4(1.f),
			FaceCullingMode::None,
			0
		);

		RendererContext.FrameBuffer.Unbind();
		PostFxShader.SetUniform("Phoenix_PostFxEnabled", false);
		RendererContext.DrawMesh(
			quadMesh,
			PostFxShader,
			{ 2.f, 2.f, 2.f },
			glm::mat4(1.f),
			FaceCullingMode::None,
			0
		);
	}

	this->OnFrameRenderGui.Fire(deltaTime);
	// Material Editor may screw up some stuff
	glViewport(0, 0, WindowSizeX, WindowSizeY);
	RendererContext.FrameBuffer.Unbind();

	{
		ZoneScopedN("DearImGuiRender");

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_FRAMEBUFFER_SRGB);
}

static void ensureDataModelValid(const ObjectHandle& DataModel)
{
	ZoneScoped;
	PHX_ENSURE_MSG(DataModel, "DataModel is NULL!");

	GameObject* workspace = DataModel->FindChild("Workspace");
	PHX_ENSURE_MSG(workspace, "DataModel has no Workspace!");
	PHX_ENSURE_MSG(workspace->FindComponent<EcWorkspace>(), "Workspace masquerading!");
	PHX_ENSURE_MSG(DataModel->FindComponent<EcDataModel>(), "DataModel masquerading!");
}

void Engine::BindDataModel(const ObjectHandle& NewDataModel)
{
    ZoneScoped;
    ensureDataModelValid(NewDataModel);

    if (DataModelRef)
    {
        EcDataModel* dm = DataModelRef->FindComponent<EcDataModel>();
        PHX_ENSURE(dm);

        dm->UnbindServices();
    }

    DataModelRef = NewDataModel;
    WorkspaceRef = NewDataModel->FindChild("Workspace");
    assert(WorkspaceRef.Dereference());

    ObjectManager.DataModel = NewDataModel->ObjectId;
    NewDataModel->FindComponent<EcDataModel>()->BindServices();
}

static void dispatchParallelVMs(Engine* engine)
{
    ZoneScoped;

    for (ScriptEngine::ParallelVM* vm : ScriptEngine::ParallelVMs)
    {
        if (vm->YieldedCoroutines.size() == 0 && vm->ParallelSpawnRequests.size() == 0)
            continue;

        engine->ThreadManagerInstance.Dispatch(
            "ParallelVM",
            [=]()
            {
                ZoneScoped;
                ZoneText(vm->Name.data(), vm->Name.size());
                vm->StepParallelScheduler(ScriptEngine::ExecutionPhase::Parallel);
            },
            true
        );
    }
}

static void parallelVMsSerialPhase()
{
    ZoneScoped;

    for (ScriptEngine::ParallelVM* vm : ScriptEngine::ParallelVMs)
        vm->StepParallelScheduler(ScriptEngine::ExecutionPhase::Serial);
}

static void waitForParallelVMs()
{
    ZoneScoped;

    while (ScriptEngine::ParallelVMsExecuting != 0)
        std::this_thread::sleep_for(std::chrono::microseconds(100));
}

void Engine::Start()
{
    if (!IsHeadlessMode)
    {
        Log.Info("Scaling...");
        updateImGuiForDisplayScaling();
    }

    Log.Info("Validating DataModel...");
    ensureDataModelValid(DataModelRef.Dereference());

    Log.Info("Final initializations...");

    ScriptEngine::Initialize(); // can only do this after datamodel is bound
	ComponentManagers.Sound.IsHeadless = IsHeadlessMode;

    double RunningTime = GetRunningTime();
    double LastFrame = RunningTime;
    double LastSecond = 0.f;

    ComponentManagers.Environment.GetService()->ChangeSkybox(ComponentManagers.Environment.GetService()->Skybox);

    m_FboResourceId = TextureManagerInstance.Assign({
        .ImagePath = "!Framebuffer:Main",
        .ResourceId = UINT32_MAX,
        .GpuId = RendererContext.FrameBuffer.GpuTextureId,
        .Width = WindowSizeX, .Height = WindowSizeY,
        .NumColorChannels = 3
    }, "!Framebuffer:Main");

    Physics::World physWorld;
    std::vector<EcParticleEmitter*> particleEmittersRenderList;

    m_IsRunning = true;
    bool firstFrame = true;

    Log.Info("Main engine loop start");

	while ((!PHX_HEADLESS_BUILD && !IsHeadlessMode) ? (!glfwWindowShouldClose(Window) && m_IsRunning) : m_IsRunning)
	{
		TIME_SCOPE_AS_N("EntireFrame", EntireFrameTimerScope);
		ZoneScopedNC("Frame", tracy::Color::PaleTurquoise);

		if (DataModelRef->IsDestructionPending)
		{
			Log.Warning("`Destroy` called on DataModel, shutting down");
			break;
		}

		if (WorkspaceRef->IsDestructionPending)
		{
			Log.Warning("`Destroy` called on Workspace, shutting down");
			break;
		}

		if (EcDataModel* dm = PrimaryDataModel->FindComponent<EcDataModel>())
		{
			if (dm->Closed)
			{
				ExitCode = dm->ExitCode;
				Close();
				// run for an additional frame to process callbacks
			}
		}
		else
		{
			Log.Warning("DataModel component removed from Primary Data Model, shutting down");
			break;
		}

		RunningTime = GetRunningTime();
		RendererContext.AccumulatedDrawCallCount = 0;

		this->FpsCap = std::clamp(this->FpsCap, 1, INT32_MAX);
		int throttledFpsCap = IsWindowFocused ? FpsCap : 10;

		double deltaTime = RunningTime - LastFrame;
		double fpsCapDelta = 1.f / throttledFpsCap;
		LastFrame = RunningTime;

		// make sure the order of timers is
		// 0 - EntireFrame, 1 - FrameSleep, 2 - FrameWork
		// `drawDeveloperUI` depends on this
		static Timing::StaticMagicTimerThing FrameSleepTimer("FrameSleep");
		static Timing::StaticMagicTimerThing FrameWorkTimer("FrameWork");

		// Wait the appropriate amount of time between frames
		if ((!VSync || !IsWindowFocused) && (Timing::FinalFrameTimes[FrameWorkTimer.TimerId] < fpsCapDelta) && !firstFrame)
		{
			Timing::ScopedTimer scoped(FrameSleepTimer.TimerId);
			ZoneScopedNC("SleepForFpsCap", tracy::Color::Wheat);

			std::this_thread::sleep_for(std::chrono::duration<double>(fpsCapDelta - Timing::FinalFrameTimes[FrameWorkTimer.TimerId]));
		}

		firstFrame = false;
		Timing::ScopedTimer framwWorkTimerScope(FrameWorkTimer.TimerId);

		DataModelRef->FindComponent<EcDataModel>()->Bind();

		if (!IsHeadlessMode)
			TextureManagerInstance.FinalizeAsyncLoadedTextures();

		MeshProviderInstance.FinalizeAsyncLoadedMeshes();
		Logging::FlushParallelEvents();

        if (EcEnvironmentService* env = ComponentManagers.Environment.GetService(); env->SkyboxFacesBeingLoaded.size() == 6 && !IsHeadlessMode)
        {
            bool skyboxLoaded = true;

            for (uint32_t skyboxFace : env->SkyboxFacesBeingLoaded)
            {
                Texture& texture = TextureManagerInstance.GetTextureResource(skyboxFace);

                if (texture.Status != Texture::LoadStatus::Succeeded)
                {
                    skyboxLoaded = false;
                    break;
                }
            }

			if (skyboxLoaded)
			{
				ZoneScopedN("UploadSkyboxToGpu");
				
				glBindTexture(GL_TEXTURE_CUBE_MAP, env->SkyboxTextureGpuId);

                for (int skyboxFaceIndex = 0; skyboxFaceIndex < 6; skyboxFaceIndex++)
                {
                    Texture& texture = TextureManagerInstance.GetTextureResource(env->SkyboxFacesBeingLoaded.at(skyboxFaceIndex));

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
                env->SkyboxFacesBeingLoaded.clear();
			}
		}

		RunningTime = GetRunningTime();
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
				ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
			else
			{
				ImGuiViewport* viewport = ImGui::GetMainViewport();
				viewport->WorkPos = OverrideViewportDockSpacePosition;
				viewport->WorkSize = OverrideViewportDockSpaceSize;

				ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
			}
		}

		EcWorkspace* workspaceComponent = WorkspaceRef->FindComponent<EcWorkspace>();
		ObjectHandle sceneCamObject = workspaceComponent->GetSceneCamera();
		EcCamera* sceneCamera = sceneCamObject->FindComponent<EcCamera>();

		if (!IsHeadlessMode)
			workspaceComponent->UpdateSoundListener();

        REFLECTION_SIGNAL_EVENT(DataModelRef->FindComponent<EcDataModel>()->OnFrameBeginCallbacks, deltaTime);

        parallelVMsSerialPhase();
        ScriptEngine::StepVMs(); // scripts may try to draw Dear ImGui, this needs to be AFTER `ImGui::NewFrame`
        dispatchParallelVMs(this); // only after serial phase!!

		// fetch the camera again because of potential CurrentScene changes that may have caused re-alloc'd
		// (really need a generic `Ref` system)
		sceneCamera = sceneCamObject->FindComponent<EcCamera>();

		EcDirectionalLight* sun = nullptr;
		{
			TIME_SCOPE_AS("TraverseHierarchy");

			CurrentScene.RenderList.clear();
			CurrentScene.LightingList.clear();
			physWorld.Dynamics.clear();
			physWorld.Statics.clear();
			particleEmittersRenderList.clear();

			// Aggregate mesh and light data into lists
			traverseHierarchy(
				CurrentScene,
				physWorld,
				particleEmittersRenderList,
				WorkspaceRef.Dereference(),
				sceneCamera,
				deltaTime,
				&sun,
				PhysicsInstance.DebugCollisionAabbs
			);

            // TODO weird skybox graphical corruption if we don't draw anything
            if (CurrentScene.RenderList.size() == 0)
                CurrentScene.RenderList.push_back(RenderItem{
                    .RenderMeshId = 1,
                    .MaterialId = MaterialManagerInstance.LoadFromPath("@base/materials/plastic.mtl"),
                    .Transparency = 1.f
                });

            if (PhysicsInstance.DebugSpatialHeat)
            {
                workspaceComponent = WorkspaceRef->FindComponent<EcWorkspace>();
                assert(workspaceComponent);

                for (const auto& it : workspaceComponent->SpatialHash)
                {
                    if (it.second.size() == 0)
                        continue;

                    CurrentScene.RenderList.push_back(RenderItem{
                        .RenderMeshId = 0,
                        .Transform = glm::translate(glm::mat4(1.f), (glm::vec3)it.first),
                        .Size = glm::vec3(SPATIAL_HASH_GRID_SIZE),
                        .MaterialId = MaterialManagerInstance.LoadFromPath("@base/materials/neon.mtl"),
                        .TintColor = glm::vec3(1.f, 0.f, 0.f),
                        .Transparency = std::clamp(1.f - ((float)(it.second.size() + 5) / 64.f), 0.2f, 1.f),
                        .FaceCulling = FaceCullingMode::None
                    });
                }
            }

            sceneCamera = sceneCamObject->FindComponent<EcCamera>();
		}

        if (PhysicsInstance.Simulating && !PhysicsInstance.SimulatingForcePaused && physWorld.Dynamics.size() > 0)
            PhysicsInstance.Step(physWorld, deltaTime * PhysicsInstance.Timescale);

        if (!IsHeadlessMode)
        {
            CurrentScene.UsedShaders.clear();

            for (const RenderItem& ri : CurrentScene.RenderList)
                CurrentScene.UsedShaders.insert(MaterialManagerInstance.GetMaterialResource(ri.MaterialId).ShaderId);

            if (particleEmittersRenderList.size() > 0)
                CurrentScene.UsedShaders.insert(ShaderManagerInstance.LoadFromPath("@base/shaders/particle.shp"));
        }

		if (!IsHeadlessMode && sun)
		{
			TIME_SCOPE_AS("Shadows");
			ZoneScopedN("Shadows");

			Scene sunScene;
			sunScene.RenderList.reserve(CurrentScene.RenderList.size());
			sunScene.UsedShaders = CurrentScene.UsedShaders;

			for (const RenderItem& ri : CurrentScene.RenderList)
				if (ri.CastsShadows)
				{
					sunScene.RenderList.push_back(ri);
					sunScene.RenderList.back().FaceCulling = FaceCullingMode::FrontFace;
				}

			glm::vec3 sunDirection = sun->Direction;
			
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

			SunShadowMap.Bind();
			glViewport(0, 0, SunShadowMapResolutionSq, SunShadowMapResolutionSq);
			glClear(/*GL_COLOR_BUFFER_BIT |*/ GL_DEPTH_BUFFER_BIT);

			for (uint32_t shdId : CurrentScene.UsedShaders)
			{
				ShaderProgram& shd = ShaderManagerInstance.GetShaderResource(shdId);
				shd.SetUniform("Phoenix_IsShadowMap", true);

				glActiveTexture(GL_TEXTURE0 + ReservedTextureSlot::Shadowmap);
				SunShadowMap.BindTexture();
				shd.SetUniform("Phoenix_ShadowAtlas", ReservedTextureSlot::Shadowmap);

				shd.SetUniform("Phoenix_DirectionalLightProjection", sunRenderMatrix);
			}

			RendererContext.DrawScene(sunScene, sunRenderMatrix, glm::mat4(1.f), RunningTime, DebugWireframeRendering);
			SunShadowMap.Unbind();

			glViewport(0, 0, WindowSizeX, WindowSizeY);
		}

		if (!IsHeadlessMode)
		{
			EcEnvironmentService* env = ComponentManagers.Environment.GetService();

            for (uint32_t shdId : CurrentScene.UsedShaders)
            {
                ShaderProgram& shader = ShaderManagerInstance.GetShaderResource(shdId);
                shader.SetUniform("Phoenix_IsShadowMap", false);
                shader.SetUniform("Phoenix_LightAmbient", env->AmbientLight.ToGenericValue());

                shader.SetUniform("Phoenix_Fog", env->Fog);
                if (env->Fog)
                    shader.SetUniform("Phoenix_FogColor", env->FogColor.ToGenericValue());
            }

			m_Render(deltaTime, particleEmittersRenderList);
			RendererContext.SwapBuffers();
		}

		waitForParallelVMs();

		// End of frame
		RunningTime = GetRunningTime();

		m_DrawnFramesInSecond++;

		OnFrameEnd.Fire(deltaTime);

		if (RunningTime - LastSecond > 1.f)
		{
			LastSecond = RunningTime;

			this->FramesPerSecond = m_DrawnFramesInSecond;
			m_DrawnFramesInSecond = -1;

			Logging::Save();
		}

		Memory::FrameFinish();
		Timing::Finish();
		FrameMark;
	}

	Log.Info("Main loop exited");
}

void Engine::Shutdown()
{
	ZoneScoped;

	Log.Info("Engine destructing...");

	Log.Info("Destroying DataModel...");
	ComponentManagers.DataModel.NotifyAllOfShutdown();
	ScriptEngine::StepVMs(); // step event callbacks

	DataModelRef->Destroy();
	WorkspaceRef->Destroy();
	DataModelRef.Clear();
	WorkspaceRef.Clear();

	for (GameObject& obj : ObjectManager.WorldArray)
	{
		if (!obj.IsDestructionPending && obj.Valid)
			obj.Destroy();
	}

	for (GameObjectManager::Collection& collection : ObjectManager.Collections)
	{
		delete collection.AddedEvent.Descriptor;
		delete collection.RemovedEvent.Descriptor;
		collection.AddedEvent.Descriptor = nullptr;
		collection.RemovedEvent.Descriptor = nullptr;
	}

	Log.Info("Shutting down script engine...");
	ScriptEngine::Shutdown();

	Log.Info("Shutting down HistoryInstance...");
	HistoryInstance.Shutdown();

	Log.Info("Shutting down Component Managers...");
	// skip the first "None" component manager
	for (size_t i = 1; i < (size_t)EntityComponent::__count; i++)
	{
		ZoneScopedN("Shutdown Component");
		ZoneText(s_EntityComponentNames[i].data(), s_EntityComponentNames[i].size());

		ObjectManager.ComponentManagers[i]->Shutdown();
	}

	Log.Info("Shutting down managers...");

	MaterialManagerInstance.Shutdown();
	TextureManagerInstance.Shutdown();
	ShaderManagerInstance.Shutdown();
	MeshProviderInstance.Shutdown();

	ThreadManagerInstance.Shutdown();

	Log.Info("Shutting down libraries...");

	if (!IsHeadlessMode)
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		RendererContext.Shutdown();
	}

	Log.Info("Shutting down GLFW...");

	if (Window)
		glfwDestroyWindow(Window);
	glfwTerminate();

	for (const GameObject& obj : ObjectManager.WorldArray)
	{
		if (obj.HardRefCount > 0)
			Log.WarningF("Object {} still has references!", obj.GetFullName());
		else if (obj.Valid)
			Log.WarningF("Object {} left dangling!", obj.GetFullName());
	}

	EngineInstance = nullptr;
}

Engine::~Engine()
{
	assert(!EngineInstance);
}
