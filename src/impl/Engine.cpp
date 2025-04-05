#include <chrono>
#include <string>
#include <format>

#include <glad/gl.h>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_sdl3.h>

#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>

#include <tracy/Tracy.hpp>

#include "Engine.hpp"

#include "component/ParticleEmitter.hpp"
#include "component/Transformable.hpp"
#include "component/Camera.hpp"
#include "component/Script.hpp"
#include "component/Light.hpp"
#include "PerformanceTiming.hpp"
#include "GlobalJsonConfig.hpp"
#include "ThreadManager.hpp"
#include "UserInput.hpp"
#include "Utilities.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

static const std::unordered_map<SDL_LogPriority, const std::string> LogPriorityStringMap =
{
	{ SDL_LOG_PRIORITY_VERBOSE, "Verbose" },
	{ SDL_LOG_PRIORITY_DEBUG, "Debug" },
	{ SDL_LOG_PRIORITY_INFO, "Info" },
	{ SDL_LOG_PRIORITY_WARN, "Warning" },
	{ SDL_LOG_PRIORITY_ERROR, "Error" },
	{ SDL_LOG_PRIORITY_CRITICAL, "Critical" },
	{ SDL_LOG_PRIORITY_COUNT, "[This string should never be displayed]" }
};

static void sdlLog(void*, int Type, SDL_LogPriority Priority, const char* Message)
{
	auto priorityIt = LogPriorityStringMap.find(Priority);

	std::string priorityName;

	if (priorityIt == LogPriorityStringMap.end())
		priorityName = std::to_string((int)Priority);
	else
		priorityName = priorityIt->second;

	std::string logString = std::vformat(
		"SDL log -\n\tType: {}\n\tPriority: {}\n\tMessage: {}",
		std::make_format_args(Type, priorityName, Message)
	);

	if (Priority < SDL_LOG_PRIORITY_ERROR)
		Log::Warning(logString);
	else
		throw(logString);
}

static Engine* EngineInstance = nullptr;

Engine* Engine::Get()
{
	return EngineInstance;
}

void Engine::ResizeWindow(int NewSizeX, int NewSizeY)
{
	ZoneScoped;

	SDL_SetWindowSize(this->Window, NewSizeX, NewSizeY);

	this->OnWindowResized(NewSizeX, NewSizeY);
}

void Engine::OnWindowResized(int NewSizeX, int NewSizeY)
{
	ZoneScoped;

	this->WindowSizeX = NewSizeX;
	this->WindowSizeY = NewSizeY;

	//m_BloomFbo.ChangeResolution(WindowSizeX, WindowSizeY);
	RendererContext.ChangeResolution(WindowSizeX, WindowSizeY);

	SDL_DisplayID currentDisplay = SDL_GetDisplayForWindow(Window);
	if (currentDisplay == 0)
		throw("`SDL_GetDisplayForWindow` failed with error: " + std::string(SDL_GetError()));

	float displayScale = SDL_GetDisplayContentScale(currentDisplay);
	if (displayScale == 0.f)
		throw("`SDL_GetDisplayContentScale` returned invalid result, error: " + std::string(SDL_GetError()));

	static ImGuiStyle DefaultStyle = ImGui::GetStyle();
	ImGuiStyle scaledStyle = DefaultStyle;
	scaledStyle.ScaleAllSizes(displayScale);
	scaledStyle.DisplayWindowPadding = ImVec2(19.f, 19.f);

	// this looks weird 02/01/2025
	ImGui::GetStyle() = scaledStyle;
}

void Engine::SetIsFullscreen(bool Fullscreen)
{
	this->IsFullscreen = Fullscreen;

	if (!SDL_SetWindowFullscreen(this->Window, this->IsFullscreen))
		throw("`SDL_SetWindowFullscreen` failed, error: " + std::string(SDL_GetError()));
}

template <class T>
static T readFromConfiguration(const std::string_view& Key, const T& DefaultValue)
{
	try
	{
		return EngineJsonConfig.value(Key, DefaultValue);
	}
	catch (nlohmann::json::type_error Error)
	{
		std::string errorMessage = Error.what();

		// 03/02/2025 "no matching token: {" WTF DO YOU MEAN
		/*
		Log::Error(std::vformat(
			"Error trying to read key '{}' of configuration: {}. Falling back to default value",
			std::make_format_args(Key, errorMessage)
		);
		*/

		return DefaultValue;
	}
}

void Engine::LoadConfiguration()
{
	bool ConfigLoadSucceeded = true;
	std::string ConfigLoadErrorMessage = "Failed to load configuration file.";
	std::string ConfigAscii = FileRW::ReadFile("./phoenix.conf", &ConfigLoadSucceeded);

	if (ConfigLoadSucceeded)
		try
		{
			EngineJsonConfig = nlohmann::json::parse(ConfigAscii);
		}
		catch (nlohmann::json::parse_error err)
		{
			ConfigLoadSucceeded = false;
			const char* errMessage = err.what();
			
			ConfigLoadErrorMessage = std::vformat(
				"Failed to load configuration file: {}\nA fallback will be used.",
				std::make_format_args(errMessage)
			);
		}

	if (!ConfigLoadSucceeded)
	{
		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_WARNING,
			"Configuration Error",
			ConfigLoadErrorMessage.c_str(),
			Window
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
		Log::Warning(std::vformat(
			"Resources Directory was changed to '{}' instead of 'resources/'. Prepare for unforeseen consequences.",
			std::make_format_args(resDir)
		));

	Log::Info("Configuration loaded");
}

Engine::Engine()
{
	EngineInstance = this;

	// 15/08/2024:
	// Hmm, this single commented-out line look like the remnants
	//  of my first attempt trying to get behavior like we have
	// `GameObject::Create` today.
	// My idea was, "Since constructors return objects, why don't I
	// just have a list of pointers to constructors"?
	// And, that's actually pretty similar to what I ended up going through
	// with. It works because they all share the same baseclass and thus
	// are implicitly downcasted. The current version just has an additional
	// layer with a templated function.
	// 
	//GameObject::GameObjectTable["Model"] = &Object_Model()

	this->LoadConfiguration();

	nlohmann::json defaultWindowSize = readFromConfiguration(
		"DefaultWindowSize",
		nlohmann::json::array({ 1280, 720 })
	);

	PHX_SDL_CALL(SDL_Init, SDL_INIT_VIDEO);

	SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
	PHX_ENSURE_MSG(primaryDisplay != 0, "`SDL_GetPrimaryDisplay` failed with error: " + std::string(SDL_GetError()));

	float displayScale = SDL_GetDisplayContentScale(primaryDisplay);
	PHX_ENSURE_MSG(displayScale != 0.f, "Invalid `SDL_GetDisplayContentScale` result, error: " + std::string(SDL_GetError()));

	this->WindowSizeX = static_cast<int>((float)defaultWindowSize[0] * displayScale);
	this->WindowSizeY = static_cast<int>((float)defaultWindowSize[1] * displayScale);

	SDL_Rect displayBounds{};
	PHX_SDL_CALL(SDL_GetDisplayBounds, primaryDisplay, &displayBounds);

	this->WindowSizeX = std::clamp(this->WindowSizeX, 1, displayBounds.w);
	this->WindowSizeY = std::clamp(this->WindowSizeY, 1, displayBounds.h);

	// This is easily the worst complaint I've had about this library,
	// the log function *does not get called when an error retrievable by `SDL_GetError` occurs*!
	// It's complete RUBBISH, USELESS, TRASH, BULLSHIT
	// 09/09/2024
	SDL_SetLogOutputFunction(sdlLog, nullptr);

	nlohmann::json::array_t requestedGLVersion = readFromConfiguration("OpenGLVersion", nlohmann::json{ 4, 6 });
	int requestedGLVersionMajor = requestedGLVersion[0];
	int requestedGLVersionMinor = requestedGLVersion[1];

	const static std::unordered_map<std::string_view, SDL_GLProfile> StringToGLProfile =
	{
		{ "Core", SDL_GL_CONTEXT_PROFILE_CORE },
		{ "Compatibility", SDL_GL_CONTEXT_PROFILE_COMPATIBILITY },
		{ "ES", SDL_GL_CONTEXT_PROFILE_ES }
	};

	std::string requestedProfileString = EngineJsonConfig.value("OpenGLProfile", "Core");

	auto requestedProfileIt = StringToGLProfile.find(requestedProfileString);
	SDL_GLProfile requestedProfile = SDL_GL_CONTEXT_PROFILE_CORE;

	if (requestedProfileIt == StringToGLProfile.end())
		Log::Warning(std::vformat(
			"Invalid/unsupported OpenGL profile '{}' requested, defaulting to the Core profile",
			std::make_format_args(requestedProfileString)
		));
	else
		requestedProfile = requestedProfileIt->second;

	Log::Info(std::vformat(
		"Requesting a {} OpenGL context with version {}.{}",
		std::make_format_args(requestedProfileString, requestedGLVersionMajor, requestedGLVersionMinor)
	));

	// Must be set *before* window creation
	PHX_SDL_CALL(
		SDL_GL_SetAttribute,
		SDL_GL_CONTEXT_PROFILE_MASK, 
		requestedProfile
	);
	PHX_SDL_CALL(
		SDL_GL_SetAttribute,
		SDL_GL_CONTEXT_MAJOR_VERSION, 
		requestedGLVersionMajor
	);
	PHX_SDL_CALL(
		SDL_GL_SetAttribute,
		SDL_GL_CONTEXT_MINOR_VERSION, 
		requestedGLVersionMinor
	);

	PHX_SDL_CALL(SDL_GL_SetAttribute, SDL_GL_DOUBLEBUFFER, 1);
	PHX_SDL_CALL(SDL_GL_SetAttribute, SDL_GL_DEPTH_SIZE, 24);
	PHX_SDL_CALL(SDL_GL_SetAttribute, SDL_GL_STENCIL_SIZE, 8);

	this->Window = SDL_CreateWindow(
		readFromConfiguration<std::string_view>("GameTitle", "PhoenixEngine").data(),
		this->WindowSizeX, this->WindowSizeY,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
	);

	PHX_ENSURE_MSG(this->Window, "SDL could not create the window: " + std::string(SDL_GetError()));

	Log::Info("Window created");

	Log::Info("Initializing systems...");

	this->RendererContext.Initialize(this->WindowSizeX, this->WindowSizeY, this->Window);
	m_BloomFbo.Initialize(WindowSizeX, WindowSizeY, 0);

	m_TextureManager.Initialize();
	m_ShaderManager.Initialize();
	m_MaterialManager.Initialize(); // mm after tm and sm as it may attempt to load a texture and shader

	m_MeshProvider.Initialize();

	Log::Info("Blue frame...");

	RendererContext.FrameBuffer.Unbind();

	glClearColor(0.07f, 0.13f, 0.17f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	RendererContext.SwapBuffers();

	Log::Info("Initializing DataModel...");

	this->DataModel = GameObject::Create("DataModel");
	GameObject::s_DataModel = DataModel->ObjectId;

	//ThreadManager::Get()->CreateWorkers(4, WorkerType::DefaultTaskWorker);

	Log::Info("Engine initialized");
}

/*
	TODO 23/12/2024
	
	We need to update all the scripts FIRST, as they may delete Objects
	that we want to keep track of (i.e. that were collected in the `PhysicsList`
	before they were deleted).

	This can be superseded by a proper (deferred) Event System, as we'd have better
	control over WHEN Scripts are resumed, instead of just resuming them
	as we stumble upon them.
*/
static void updateScripts(double DeltaTime)
{
	ZoneScopedC(tracy::Color::LightSkyBlue);

	static std::vector<GameObjectRef> ScriptsResumedThisFrame = {};

	for (GameObject* ch : Engine::Get()->DataModel->GetDescendants())
		if (ch->Enabled)
			if (EcScript* script = ch->GetComponent<EcScript>())
			{
				if (std::find(
					ScriptsResumedThisFrame.begin(),
					ScriptsResumedThisFrame.end(),
					ch
				) == ScriptsResumedThisFrame.end())
				{
					// ensure we keep a reference to it
					ScriptsResumedThisFrame.emplace_back(ch);

					script->Update(DeltaTime);

					// we need to do this in case a script deletes another script,
					// causing their to potentially be stale pointers in the list
					// of descendants we are iterating, or if it just deletes
					// parts of the datamodel we are about to iterate
					updateScripts(DeltaTime);

					return;
				}
			}

	ScriptsResumedThisFrame.clear();
}

static bool s_DebugCollisionAabbs = false;

static void recursivelyTravelHierarchy(
	std::vector<RenderItem>& RenderList,
	std::vector<LightItem>& LightList,
	std::vector<GameObject*>& PhysicsList,
	GameObjectRef Root,
	EcCamera* SceneCamera,
	double DeltaTime
)
{
	ZoneScopedC(tracy::Color::LightGoldenrod);

	static uint32_t wireframeMaterial = MaterialManager::Get()->LoadMaterialFromPath("wireframe");
	static uint32_t cubeMesh = MeshProvider::Get()->LoadFromPath("!Cube");

	std::vector<GameObject*> objectsRaw = Root->GetChildren();
	// TODO 05/04/2025: FIXME
	std::vector<GameObjectRef> objects;
	objects.reserve(objectsRaw.size());

	for (GameObject* o : objectsRaw)
		objects.emplace_back(o);

	for (GameObjectRef object : objects)
	{
		if (!object->Enabled)
			continue;
			
		EcMesh* object3D = object->GetComponent<EcMesh>();
		EcTransformable* ct = object->GetComponent<EcTransformable>();

		if (object3D)
		{
			if (object3D->PhysicsDynamics || object3D->PhysicsCollisions)
				PhysicsList.emplace_back(object);

			if (object3D->Transparency > .95f)
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
					wireframeMaterial,
					object3D->Tint,
					0.f,
					0.f,
					0.f,
					FaceCullingMode::None,
					false
				);
		}

		EcDirectionalLight* directional = object->GetComponent<EcDirectionalLight>();
		EcPointLight* point = object->GetComponent<EcPointLight>();
		EcSpotLight* spot = object->GetComponent<EcSpotLight>();
		
		if (directional)
			LightList.emplace_back(
				LightType::Directional,
				directional->Shadows,
				(glm::vec3)ct->Transform[3],
				directional->LightColor * directional->Brightness
			);

		if (point)
			LightList.emplace_back(
				LightType::Point,
				point->Shadows,
				(glm::vec3)ct->Transform[3],
				point->LightColor * point->Brightness,
				point->Range
			);

		if (spot)
			LightList.emplace_back(
				LightType::Spot,
				spot->Shadows,
				(glm::vec3)ct->Transform[3],
				spot->LightColor * spot->Brightness,
				spot->Range,
				spot->Angle
			);

		if (!object->GetChildren().empty())
			recursivelyTravelHierarchy(
				RenderList,
				LightList,
				PhysicsList,
				object,
				SceneCamera,
				DeltaTime
			);

		EcParticleEmitter* emitter = object->GetComponent<EcParticleEmitter>();

		if (emitter)
		{
			emitter->Update(DeltaTime);
			emitter->AppendToRenderList(RenderList);
		}
	}
}

void Engine::Start()
{
	Log::Info("Final initializations...");

	MaterialManager* mtlManager = MaterialManager::Get();
	TextureManager* texManager = TextureManager::Get();
	MeshProvider* meshProvider = MeshProvider::Get();

	const Mesh cubeMesh = meshProvider->GetMeshResource(meshProvider->LoadFromPath("!Cube"));
	const Mesh quadMesh = meshProvider->GetMeshResource(meshProvider->LoadFromPath("!Quad"));

	double RunningTime = GetRunningTime();

	// `LastFrameBegan` is for deltatime
	// `LastFrameEnded` is for FPS cap
	double LastFrameBegan = RunningTime;
	double LastFrameEnded = RunningTime;
	double LastSecond = 0.f;

	static const std::string SkyPath = "textures/Sky1/";

	static const std::string SkyboxCubemapImages[6] =
	{
		"right",
		"left",
		"top",
		"bottom",
		"front",
		"back"
	};

	std::vector<uint32_t> skyboxFacesBeingLoaded;

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
		const std::string& skyboxImage = SkyboxCubemapImages[faceIndex];

		uint32_t tex = texManager->LoadTextureFromPath(SkyPath + skyboxImage + ".jpg");
		skyboxFacesBeingLoaded.push_back(tex);

		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
			0,
			GL_RGB,
			1,
			1,
			0,
			GL_RED,
			GL_UNSIGNED_BYTE,
			texManager->GetTextureResource(tex).TMP_ImageByteData
		);

		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	//Texture HDRISkyboxTexture("Assets/Textures/hdri_sky_1.jpeg", "Diffuse", 0);

	ShaderManager* shdManager = ShaderManager::Get();

	uint32_t postFxShaderId = shdManager->LoadFromPath("postprocessing");
	uint32_t skyboxShaderId = shdManager->LoadFromPath("skybox");
	uint32_t bloomExtractShaderId = shdManager->LoadFromPath("bloomingextract");
	uint32_t bloomCompositeShaderId = shdManager->LoadFromPath("bloomcomposite");
	uint32_t separableBlurShaderId = shdManager->LoadFromPath("separableblur");

	// we intentionally perform a copy here, because if another shader gets loaded,
	// the entire list can get re-allocated, and the references will break and just be
	// garbage data.
	ShaderProgram postFxShaders = shdManager->GetShaderResource(postFxShaderId);
	ShaderProgram skyboxShaders = shdManager->GetShaderResource(skyboxShaderId);
	ShaderProgram bloomExtractShaders = shdManager->GetShaderResource(bloomExtractShaderId);
	ShaderProgram bloomCompositeShaders = shdManager->GetShaderResource(bloomCompositeShaderId);
	ShaderProgram separableBlurShaders = shdManager->GetShaderResource(separableBlurShaderId);

	postFxShaders.SetUniform("Texture", 1);
	postFxShaders.SetUniform("DistortionTexture", 2);
	postFxShaders.SetUniform("BloomTexture", 3);
	bloomExtractShaders.SetUniform("Texture", 1);
	bloomCompositeShaders.SetUniform("Texture", 3);
	separableBlurShaders.SetUniform("Texture", 3);

	skyboxShaders.SetUniform("SkyboxCubemap", 3);

	Scene scene{};
	scene.RenderList.reserve(50);

	RendererContext.FrameBuffer.Unbind();
	
	uint32_t distortionTexture = texManager->LoadTextureFromPath("textures/screendistort.jpg");

	SDL_Event pollingEvent;

	const int32_t SunShadowMapResolutionSq = 512;
	GpuFrameBuffer sunShadowMap{ SunShadowMapResolutionSq, SunShadowMapResolutionSq };

	m_IsRunning = true;

	Log::Info("Main engine loop start");

	while (m_IsRunning)
	{
		ZoneScopedN("Frame");

		if (DataModel->IsDestructionPending)
		{
			Log::Warning("`::Destroy` called on DataModel, shutting down");
			break;
		}

		EcDataModel* cdm = DataModel->GetComponent<EcDataModel>();

		GameObject* workspace = DataModel->FindChild("Workspace");

		if (!workspace)
		{
			Log::Warning("Workspace was removed, shutting down");
			break;
		}

		this->Workspace = workspace;

		if (cdm->WantExit)
		{
			Log::Info("DataModel requested shutdown");
			break;
		}

		// so we don't need to do additional checks past this point in the scope 11/01/2025
		GameObjectRef keepWorkspace{ workspace };

		RunningTime = GetRunningTime();
		RendererContext.AccumulatedDrawCallCount = 0;
		
		static bool IsWindowFocused = true;

		this->FpsCap = std::clamp(this->FpsCap, 1, 600);
		int throttledFpsCap = IsWindowFocused ? FpsCap : 10;

		double deltaTime = RunningTime - LastFrameEnded;
		double fpsCapDelta = 1.f / throttledFpsCap;
		double lastFrameTime = Timing::FinalFrameTimes[(uint8_t)Timing::Timer::EntireFrame];

		// Wait the appropriate amount of time between frames
		if ((!VSync || !IsWindowFocused) && (deltaTime < fpsCapDelta))
		{
			ZoneScopedNC("SleepForFpsCap", tracy::Color::Wheat);
			std::this_thread::sleep_for(std::chrono::duration<double>(fpsCapDelta - deltaTime - lastFrameTime));
		}

		TIME_SCOPE_AS(Timing::Timer::EntireFrame);

		scene.RenderList.clear();
		scene.LightingList.clear();

		texManager->FinalizeAsyncLoadedTextures();
		meshProvider->FinalizeAsyncLoadedMeshes();

		if (skyboxFacesBeingLoaded.size() == 6)
		{
			bool skyboxLoaded = true;

			for (uint32_t skyboxFace : skyboxFacesBeingLoaded)
			{
				Texture& texture = texManager->GetTextureResource(skyboxFace);

				if (texture.Status != Texture::LoadStatus::Succeeded)
				{
					skyboxLoaded = false;
					break;
				}
			}

			if (skyboxLoaded)
			{
				glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);

				for (int skyboxFaceIndex = 0; skyboxFaceIndex < 6; skyboxFaceIndex++)
				{
					Texture& texture = texManager->GetTextureResource(skyboxFacesBeingLoaded.at(skyboxFaceIndex));

					glTexImage2D(
						GL_TEXTURE_CUBE_MAP_POSITIVE_X + skyboxFaceIndex,
						0,
						GL_RGB,
						texture.Width,
						texture.Height,
						0,
						GL_RGB,
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
			ZoneScopedN("PollEvents");

			while (SDL_PollEvent(&pollingEvent) != 0)
			{
				ZoneScopedN("Event");
				ZoneTextF("Type: %d", pollingEvent.common.type);

				ImGui_ImplSDL3_ProcessEvent(&pollingEvent);

				switch (pollingEvent.type)
				{

				case SDL_EVENT_QUIT:
				{
					m_IsRunning = false;
					break;
				}

				case SDL_EVENT_WINDOW_RESIZED:
				{
					int NewSizeX = pollingEvent.window.data1;
					int NewSizeY = pollingEvent.window.data2;

					// Only call ChangeResolution if the new resolution is actually different
					//if (NewSizeX != this->WindowSizeX || NewSizeY != this->WindowSizeY)
						this->OnWindowResized(NewSizeX, NewSizeY);

					break;
				}

				case SDL_EVENT_WINDOW_FOCUS_LOST:
				{
					IsWindowFocused = false;
					break;
				}

				case SDL_EVENT_WINDOW_FOCUS_GAINED:
				{
					IsWindowFocused = true;
					break;
				}

				}
			}
		}

		// so scripts can use the `imgui_*` APIs
		// 09/11/2024
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		float aspectRatio = (float)this->WindowSizeX / (float)this->WindowSizeY;

		EcCamera* sceneCamera = Workspace->GetComponent<EcWorkspace>()->GetSceneCamera()->GetComponent<EcCamera>();

		std::vector<GameObject*> physicsList;

		updateScripts(deltaTime);

		s_DebugCollisionAabbs = this->DebugAabbs;

		// Aggregate mesh and light data into lists
		recursivelyTravelHierarchy(
			scene.RenderList,
			scene.LightingList,
			physicsList,
			Workspace,
			sceneCamera,
			deltaTime
		);

		bool hasPhysics = false;

		for (GameObject* object : physicsList)
			if (object->GetComponent<EcMesh>()->PhysicsDynamics)
			{
				hasPhysics = true;
				break;
			}

		if (hasPhysics)
			Physics::Step(physicsList, deltaTime);

		bool hasSun = false;
		glm::vec3 sunDirection{ .5f, .5f, .5f };

		for (const LightItem& light : scene.LightingList)
			if (light.Type == LightType::Directional && light.Shadows)
			{
				hasSun = true;
				sunDirection = light.Position;
				break;
			}

		// we do this AFTER  `recursivelyTravelHierarchy` in case any Scripts
		// update the camera transform
		glm::mat4 renderMatrix = sceneCamera->GetMatrixForAspectRatio(aspectRatio);

		scene.UsedShaders.clear();

		for (const RenderItem& ri : scene.RenderList)
			scene.UsedShaders.insert(mtlManager->GetMaterialResource(ri.MaterialId).ShaderId);

		if (hasSun)
		{
			TIME_SCOPE_AS(Timing::Timer::Rendering);
			ZoneScopedN("Shadows");

			Scene sunScene{};
			sunScene.RenderList.reserve(scene.RenderList.size());
			sunScene.UsedShaders = scene.UsedShaders;

			for (const RenderItem& ri : scene.RenderList)
				if (ri.CastsShadows)
					sunScene.RenderList.push_back(ri);

			glm::mat4 sunOrtho = glm::ortho(-35.f, 35.f, -35.f, 35.f, 0.1f, 75.f);
			glm::mat4 sunView = glm::lookAt(50.f * sunDirection, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
			glm::mat4 sunRenderMatrix = sunOrtho * sunView;

			sunShadowMap.Bind();
			glViewport(0, 0, SunShadowMapResolutionSq, SunShadowMapResolutionSq);
			glClear(/*GL_COLOR_BUFFER_BIT |*/ GL_DEPTH_BUFFER_BIT);

			for (uint32_t shdId : scene.UsedShaders)
			{
				ShaderProgram& shd = shdManager->GetShaderResource(shdId);
				shd.SetUniform("IsShadowMap", true);

				glActiveTexture(GL_TEXTURE0 + 101);
				sunShadowMap.BindTexture();
				shd.SetUniform("ShadowAtlas", 101);

				shd.SetUniform("DirecLightProjection", sunRenderMatrix);
			}

			RendererContext.DrawScene(sunScene, sunRenderMatrix, glm::mat4(1.f), RunningTime);

			for (uint32_t shdId : scene.UsedShaders)
				shdManager->GetShaderResource(shdId).SetUniform("IsShadowMap", false);

			glViewport(0, 0, WindowSizeX, WindowSizeY);
		}

		glm::mat4 skyRenderMatrix{ 1.f };

		glm::vec3 camPos = glm::vec3(sceneCamera->Transform[3]);
		glm::vec3 camForward = glm::vec3(sceneCamera->Transform[2]);
		glm::vec3 camUp = glm::vec3(sceneCamera->Transform[1]);

		glm::mat4 view = glm::lookAt(camPos, camPos + camForward, camUp);
		glm::mat4 projection = glm::perspective(
			glm::radians(sceneCamera->FieldOfView),
			aspectRatio,
			sceneCamera->NearPlane,
			sceneCamera->FarPlane
		);;

		// "We make the mat4 into a mat3 and then a mat4 again in order to get rid of the last row and column
		// The last row and column affect the translation of the skybox (which we don't want to affect)"
		//view = glm::mat4(glm::mat3(glm::lookAt(camPos, camPos + camForward, glm::vec3(0.f, 1.f, 0.f))));
		// ...
		// ...
		// ...
		// Wow Mr Victor Gordan sir, that sounds really complicated.
		// It's really too bad there isn't a way simpler, 300x more understandable way
		// of zeroing-out the first 3 values of the last column of what is literally 4 `vec4`s that represent
		// a 4x4 matrix...
		view[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);

		skyRenderMatrix = projection * view;

		skyboxShaders.SetUniform("RenderMatrix", skyRenderMatrix);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);

		RendererContext.FrameBuffer.Bind();

		glClear(/*GL_COLOR_BUFFER_BIT |*/ GL_DEPTH_BUFFER_BIT);

		glDepthFunc(GL_LEQUAL);

		RendererContext.DrawMesh(
			cubeMesh,
			skyboxShaders,
			Vector3::one,
			skyRenderMatrix,
			FaceCullingMode::FrontFace // Cull the Outside, not the Inside
		);

		glDepthFunc(GL_LESS);

		//Main render pass
		RendererContext.DrawScene(scene, renderMatrix, sceneCamera->Transform, RunningTime);

		glDisable(GL_DEPTH_TEST);

		// 24/01/2025 `Memory` namespace can't handle
		// placement new rn, which is why i assume the counter is
		// always 0
		// use the tag MEMPLACEMENTNEW to easily grep where i do this hack
		// when this can be avoided in the future :)
		size_t* renderMemCounter = &Memory::Counters[(size_t)Memory::Category::RenderCommands];
		size_t actualRenderMem = scene.RenderList.size() * sizeof(RenderItem) + scene.LightingList.size() * sizeof(LightItem);

		*renderMemCounter += actualRenderMem;

		this->OnFrameRenderGui.Fire(deltaTime);

		*renderMemCounter -= actualRenderMem;

		{
			ZoneScopedN("DearImGuiRender");
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		glEnable(GL_DEPTH_TEST);

		//Do framebuffer stuff after everything is drawn

		RendererContext.FrameBuffer.Unbind();

		glActiveTexture(GL_TEXTURE1);
		RendererContext.FrameBuffer.BindTexture();

		glDisable(GL_DEPTH_TEST);

		if (EngineJsonConfig.value("postfx_enabled", false))
		{
			ZoneScopedN("ApplyPostFxSettings");

			postFxShaders.SetUniform("PostFxEnabled", 1);
			postFxShaders.SetUniform(
				"ScreenEdgeBlurEnabled",
				EngineJsonConfig.value("postfx_blurvignette", false)
			);
			postFxShaders.SetUniform(
				"DistortionEnabled",
				EngineJsonConfig.value("postfx_distortion", false)
			);

			postFxShaders.SetUniform(
				"Gamma",
				EngineJsonConfig.value("postfx_gamma", 1.f)
			);
			postFxShaders.SetUniform(
				"LdMax",
				EngineJsonConfig.value("postfx_ldmax", 1.f)
			);
			postFxShaders.SetUniform(
				"ContrastMax",
				EngineJsonConfig.value("postfx_cmax", 1.f)
			);
			skyboxShaders.SetUniform(
				"HdrEnabled",
				true
			);

			if (EngineJsonConfig.find("postfx_blurvignette_blurstrength") != EngineJsonConfig.end())
			{
				postFxShaders.SetUniform(
					"BlurVignetteStrength",
					(float)EngineJsonConfig["postfx_blurvignette_blurstrength"]
				);
				postFxShaders.SetUniform(
					"BlurVignetteDistMul",
					(float)EngineJsonConfig["postfx_blurvignette_weightmul"]
				);
				postFxShaders.SetUniform(
					"BlurVignetteDistExp",
					(float)EngineJsonConfig["postfx_blurvignette_weightexp"]
				);
				postFxShaders.SetUniform(
					"BlurVignetteSampleRadius",
					(int)EngineJsonConfig["postfx_blurvignette_sampleradius"]
				);
			}

			postFxShaders.SetUniform("Time", RunningTime);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, texManager->GetTextureResource(distortionTexture).GpuId);

			glActiveTexture(GL_TEXTURE3);
			m_BloomFbo.BindTexture();

			{
				ZoneScopedN("Bloom");

				//m_BloomFbo.Bind();

				{
					ZoneScopedN("Extract");
					RendererContext.DrawMesh(
						quadMesh,
						bloomExtractShaders,
						Vector3::one * 2.f,
						glm::mat4(1.f),
						FaceCullingMode::BackFace,
						0
					);

					m_BloomFbo.BindTexture();
					glGenerateMipmap(GL_TEXTURE_2D);
				}

				{
					ZoneScopedN("BlurMips");

					for (int i = 1; i < 8; i++)
					{
						glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BloomFbo.GpuTextureId, i);
						glViewport(0, 0, static_cast<int>(ceil(WindowSizeX / pow(2.f, i))), static_cast<int>(ceil(WindowSizeY / pow(2.f, i))));

						separableBlurShaders.SetUniform("LodLevel", i);

						separableBlurShaders.SetUniform("BlurXAxis", true);
						RendererContext.DrawMesh(
							quadMesh,
							separableBlurShaders,
							Vector3::one * 2.f,
							glm::mat4(1.f),
							FaceCullingMode::BackFace,
							0
						);

						separableBlurShaders.SetUniform("BlurXAxis", false);
						RendererContext.DrawMesh(
							quadMesh,
							separableBlurShaders,
							Vector3::one * 2.f,
							glm::mat4(1.f),
							FaceCullingMode::BackFace,
							0
						);
					}

					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BloomFbo.GpuTextureId, 0);
					glViewport(0, 0, WindowSizeX, WindowSizeY);
				}

				{
					ZoneScopedN("Composite");

					RendererContext.DrawMesh(
						quadMesh,
						bloomCompositeShaders,
						Vector3::one * 2.f,
						glm::mat4(1.f),
						FaceCullingMode::BackFace,
						0
					);

					//m_BloomFbo.Unbind();
				}
			}
		}
		else
		{
			postFxShaders.SetUniform("PostFxEnabled", 0);
			skyboxShaders.SetUniform(
				"HdrEnabled",
				false
			);
		}

		{
			ZoneScopedN("MainPostProcessing");
			RendererContext.DrawMesh(
				quadMesh,
				postFxShaders,
				Vector3::one * 2.f,
				glm::mat4(1.f),
				FaceCullingMode::BackFace,
				0
			);
		}

		glEnable(GL_DEPTH_TEST);

		// End of frame

		RendererContext.SwapBuffers();

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

		Timing::Finish();
		FrameMark;
	}

	Log::Info("Main loop exited");
}

Engine::~Engine()
{
	Log::Info("Engine destructing...");

	Log::Save();

	Log::Info("Destroying DataModel...");

	// TODO:
	// 27/08/2024:
	// Even if I call the destructors here and clear the vector as well,
	// C++ *still* calls them again at program exit as the scope terminates.
	// It doesn't cause a use-after-free, YET
	this->DataModel->Destroy();
	GameObject::s_DataModel = PHX_GAMEOBJECT_NULL_ID;

	EngineInstance = nullptr;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();

	SDL_Quit();
}
