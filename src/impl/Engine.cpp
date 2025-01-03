#include <chrono>
#include <string>
#include <format>
#include <SDL2/SDL.h>
#include <glad/gl.h>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include "Engine.hpp"

#include "gameobject/GameObjects.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/MeshProvider.hpp"
#include "GlobalJsonConfig.hpp"
#include "ThreadManager.hpp"
#include "UserInput.hpp"
#include "Utilities.hpp"
#include "Profiler.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

// TODO: move 13/08/2024
static int WindowSizeXBeforeFullscreen = 800;
static int WindowSizeYBeforeFullscreen = 800;
static int WindowPosXBeforeFullscreen = SDL_WINDOWPOS_CENTERED;
static int WindowPosYBeforeFullscreen = SDL_WINDOWPOS_CENTERED;

static const std::unordered_map<SDL_LogPriority, const std::string> LogPriorityStringMap =
{
	{ SDL_LOG_PRIORITY_VERBOSE, "Verbose" },
	{ SDL_LOG_PRIORITY_DEBUG, "Debug" },
	{ SDL_LOG_PRIORITY_INFO, "Info" },
	{ SDL_LOG_PRIORITY_WARN, "Warning" },
	{ SDL_LOG_PRIORITY_ERROR, "Error" },
	{ SDL_LOG_PRIORITY_CRITICAL, "Critical" },
	{ SDL_NUM_LOG_PRIORITIES, "[This string should never be displayed]" }
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
		"SDL log -\nType: {}\nPriority: {}\nMessage: {}",
		std::make_format_args(Type, priorityName, Message)
	);

	if (Priority < SDL_LOG_PRIORITY_ERROR)
		Log::Warning(logString);
	else
		throw(logString);
}

static EngineObject* EngineInstance = nullptr;

EngineObject* EngineObject::Get()
{
	return EngineInstance;
}

void EngineObject::ResizeWindow(int NewSizeX, int NewSizeY)
{
	SDL_SetWindowSize(this->Window, NewSizeX, NewSizeY);

	this->OnWindowResized(NewSizeX, NewSizeY);
}

void EngineObject::OnWindowResized(int NewSizeX, int NewSizeY)
{
	this->WindowSizeX = NewSizeX;
	this->WindowSizeY = NewSizeY;

	RendererContext.ChangeResolution(WindowSizeX, WindowSizeY);
}

void EngineObject::SetIsFullscreen(bool Fullscreen)
{
	this->IsFullscreen = Fullscreen;

	if (Fullscreen)
	{
		WindowSizeXBeforeFullscreen = this->WindowSizeX;
		WindowSizeYBeforeFullscreen = this->WindowSizeY;

		SDL_GetWindowPosition(this->Window, &WindowPosXBeforeFullscreen, &WindowPosYBeforeFullscreen);

		// SDL_SetWindowFullscreen will trigger a Resize event and call ::OnWindowResized down the pipeline.
	}
	else
	{
		SDL_SetWindowPosition(this->Window, WindowPosXBeforeFullscreen, WindowPosYBeforeFullscreen);
		this->ResizeWindow(WindowSizeXBeforeFullscreen, WindowSizeYBeforeFullscreen);
	}

	SDL_SetWindowFullscreen(this->Window, this->IsFullscreen ? SDL_WINDOW_FULLSCREEN : m_SDLWindowFlags);
}

void EngineObject::LoadConfiguration()
{
	bool ConfigFileFound = true;
	std::string ConfigAscii = FileRW::ReadFile("./phoenix.conf", &ConfigFileFound);

	if (ConfigFileFound)
	{
		try
		{
			EngineJsonConfig = nlohmann::json::parse(ConfigAscii);
		}
		catch (nlohmann::json::parse_error err)
		{
			auto errmsg = err.what();
			throw(std::vformat("Parse error while loading configuration: {}", std::make_format_args(errmsg)));
		}
	}
	else
		throw("Could not find configuration file (phoenix.conf)!");

	if (ConfigAscii == "")
		throw("Configuration file is empty");

	Log::Info("Configuration loaded");
}

void EngineObject::Initialize()
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

	nlohmann::json defaultWindowSize = EngineJsonConfig.value(
		"DefaultWindowSize",
		nlohmann::json::array({ 1280, 720 })
	);

	this->WindowSizeX = defaultWindowSize[0], this->WindowSizeY = defaultWindowSize[1];

	PHX_SDL_CALL(SDL_Init, SDL_INIT_VIDEO);

	// This is easily the worst complaint I've had about this library,
	// the log function *does not called even when an error retrievable by SDL_GetError occurs*!
	// It's complete RUBBISH, USELESS, TRASH, BULLSHIT
	// 09/09/2024
	SDL_LogSetOutputFunction(sdlLog, nullptr);

	nlohmann::json::array_t requestedGLVersion = EngineJsonConfig.value("OpenGLVersion", nlohmann::json{ 4, 6 });
	int requestedGLVersionMajor = requestedGLVersion[0];
	int requestedGLVersionMinor = requestedGLVersion[1];

	const static std::unordered_map<std::string, SDL_GLprofile> StringToGLProfile =
	{
		{ "Core", SDL_GL_CONTEXT_PROFILE_CORE },
		{ "Compatibility", SDL_GL_CONTEXT_PROFILE_COMPATIBILITY },
		{ "ES", SDL_GL_CONTEXT_PROFILE_ES }
	};

	std::string requestedProfileString = EngineJsonConfig.value("OpenGLProfile", "Core");

	auto requestedProfileIt = StringToGLProfile.find(requestedProfileString);
	SDL_GLprofile requestedProfile = SDL_GL_CONTEXT_PROFILE_CORE;

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
		EngineJsonConfig.value("GameTitle", "PhoenixEngine").c_str(),
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		this->WindowSizeX, this->WindowSizeY,
		m_SDLWindowFlags
	);

	if (!this->Window)
	{
		const char* errStr = SDL_GetError();
		throw(std::vformat("SDL could not create the window: {}\n", std::make_format_args(errStr)));
	}

	Log::Info("Window created");

	// TODO: Engine->MSAASamples does nothing, attempting to specify via below ctor's argument leads to
	// OpenGL error "Target doesn't match the texture's target"
	this->RendererContext.Initialize(this->WindowSizeX, this->WindowSizeY, this->Window);

	Log::Info("Creating initial DataModel...");

	this->DataModel = (Object_DataModel*)GameObject::Create("DataModel");
	GameObject::s_DataModel = this->DataModel;

	GameObject* workspace = DataModel->GetChild("Workspace");
	this->Workspace = (Object_Workspace*)workspace;

	//ThreadManager::Get()->CreateWorkers(4, WorkerType::DefaultTaskWorker);

	Log::Info("Engine constructed");
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
	static std::vector<GameObjectRef<Object_Script>> ScriptsResumedThisFrame = {};
	//ScriptsResumedThisFrame.reserve(2);

	for (GameObject* ch : GameObject::s_DataModel->GetDescendants())
		if (ch->Enabled)
			if (Object_Script* script = dynamic_cast<Object_Script*>(ch))
			{
				if (std::find(
					ScriptsResumedThisFrame.begin(),
					ScriptsResumedThisFrame.end(),
					script
				) == ScriptsResumedThisFrame.end())
				{
					// ensure we keep a reference to it
					ScriptsResumedThisFrame.emplace_back(script);

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

static void recursivelyTravelHierarchy(
	std::vector<RenderItem>& RenderList,
	std::vector<LightItem>& LightList,
	std::vector<Object_Base3D*>& PhysicsList,
	GameObject* Root,
	Object_Camera* SceneCamera,
	double DeltaTime
)
{
	std::vector<GameObject*> objects = Root->GetChildren();

	RenderList.reserve(RenderList.capacity() + static_cast<size_t>(objects.size() / 2));
	LightList.reserve(LightList.capacity() + static_cast<size_t>(objects.size() / 4));
	PhysicsList.reserve(PhysicsList.capacity() + std::min(static_cast<size_t>(objects.size() / 8), 512ULL));

	for (GameObject* object : objects)
	{
		if (!object->Enabled)
			continue;

		// scripts would have already been `::Update`'d by `updateScripts`
		if (!dynamic_cast<Object_Script*>(object))
			object->Update(DeltaTime);

		Object_Base3D* object3D = dynamic_cast<Object_Base3D*>(object);

		if (object3D)
		{
			if (object3D->PhysicsDynamics || object3D->PhysicsCollisions)
				PhysicsList.emplace_back(object3D);

			if (object3D->Transparency > .95f)
				continue;

			// TODO: frustum culling

			RenderList.emplace_back(
				object3D->RenderMeshId,
				object3D->Transform,
				object3D->Size,
				object3D->MaterialId,
				object3D->Tint,
				object3D->Transparency,
				object3D->MetallnessFactor,
				object3D->RoughnessFactor,
				object3D->FaceCulling,
				object3D->CastsShadows
			);
		}

		Object_Light* light = !object3D ? dynamic_cast<Object_Light*>(object) : nullptr;

		if (light)
		{
			Object_DirectionalLight* directional = dynamic_cast<Object_DirectionalLight*>(light);
			Object_PointLight* point = dynamic_cast<Object_PointLight*>(light);
			Object_SpotLight* spot = dynamic_cast<Object_SpotLight*>(light);

			if (directional)
				LightList.emplace_back(
					LightType::Directional,
					light->Shadows,
					(glm::vec3)light->LocalTransform[3],
					light->LightColor * light->Brightness
				);

			if (point)
				LightList.emplace_back(
					LightType::Point,
					light->Shadows,
					(glm::vec3)light->GetWorldTransform()[3],
					light->LightColor * light->Brightness,
					point->Range
				);

			if (spot)
				LightList.emplace_back(
					LightType::Spot,
					light->Shadows,
					(glm::vec3)light->GetWorldTransform()[3],
					light->LightColor * light->Brightness,
					spot->Range,
					spot->Angle
				);
		}

		if (!object->GetChildren().empty())
			recursivelyTravelHierarchy(
				RenderList,
				LightList,
				PhysicsList,
				object,
				SceneCamera,
				DeltaTime
			);

		Object_ParticleEmitter* emitter = (!light && !object3D) ? dynamic_cast<Object_ParticleEmitter*>(object) : nullptr;

		if (emitter)
		{
			auto pdrawlist = emitter->GetRenderList();
			std::copy(pdrawlist.begin(), pdrawlist.end(), std::back_inserter(RenderList));
		}
	}
}

void EngineObject::Start()
{
	Log::Info("Final initializations...");

	// TODO:
	// wtf are these
	// 13/07/2024
	double LastTime = this->RunningTime;
	double LastFrame = GetRunningTime();
	double FrameStart = 0.0f;
	double LastSecond = LastFrame;

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
	//glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_LOD_BIAS, 15.f);

	TextureManager* texManager = TextureManager::Get();

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

	// we intentionally perform a copy here, because if another shader gets loaded,
	// the entire list can get re-allocated, and the references will break and just be
	// garbage data.
	ShaderProgram postFxShaders = shdManager->GetShaderResource(postFxShaderId);
	ShaderProgram skyboxShaders = shdManager->GetShaderResource(skyboxShaderId);

	postFxShaders.SetUniform("Texture", 1);
	postFxShaders.SetUniform("DistortionTexture", 2);

	skyboxShaders.SetUniform("SkyboxCubemap", 3);

	Scene scene{};

	RendererContext.FrameBuffer.Unbind();
	
	uint32_t distortionTexture = texManager->LoadTextureFromPath("textures/screendistort.jpg", false);

	SDL_Event pollingEvent;

	const int32_t SunShadowMapResolutionSq = 512;
	GpuFrameBuffer sunShadowMap{ SunShadowMapResolutionSq, SunShadowMapResolutionSq };

	Log::Info("Main engine loop start");

	while (!this->Exit)
	{
		if (this->DataModel->WantExit)
		{
			Log::Info("DataModel requested shutdown");
			break;
		}

		if (!DataModel->GetChildOfClass("Workspace"))
		{
			Log::Warning("Workspace was removed, shutting down");
			break;
		}

		this->RunningTime = GetRunningTime();
		EngineJsonConfig["renderer_drawcallcount"] = 0;

		this->FpsCap = std::clamp(this->FpsCap, 1, 600);

		double frameDelta = RunningTime - LastFrame;
		double fpsCapDelta = 1.f / this->FpsCap;

		// Wait the appropriate amount of time between frames
		if (!VSync && (frameDelta < fpsCapDelta))
		{
			SDL_Delay(static_cast<uint32_t>((fpsCapDelta - frameDelta) * 1000));
			continue;
		}

		Profiler::ResetAll();

		Profiler::Start("Frame");

		scene.RenderList.clear();
		scene.LightingList.clear();

		TextureManager::Get()->FinalizeAsyncLoadedTextures();
		MeshProvider::Get()->FinalizeAsyncLoadedMeshes();

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

		if (skyboxLoaded && skyboxFacesBeingLoaded.size() == 6)
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

				glGenerateMipmap(GL_TEXTURE_2D);

				free(texture.TMP_ImageByteData);
				texture.TMP_ImageByteData = nullptr;
			}

			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

			skyboxFacesBeingLoaded.clear();
		}

		double deltaTime = GetRunningTime() - LastTime;
		LastTime = RunningTime;
		FrameStart = RunningTime;

		PROFILE_EXPRESSION("EventCallbacks/OnFrameStart", this->OnFrameStart.Fire(deltaTime));

		while (SDL_PollEvent(&pollingEvent) != 0)
		{
			PROFILE_SCOPE("PollEvents");

			ImGui_ImplSDL2_ProcessEvent(&pollingEvent);

			switch (pollingEvent.type)
			{

			case (SDL_QUIT):
			{
				this->Exit = true;
				break;
			}

			case (SDL_WINDOWEVENT):
			{
				switch (pollingEvent.window.event)
				{

				case (SDL_WINDOWEVENT_RESIZED):
				{
					int NewSizeX = pollingEvent.window.data1;
					int NewSizeY = pollingEvent.window.data2;

					// Only call ChangeResolution if the new resolution is actually different
					if (NewSizeX != this->WindowSizeX || NewSizeY != this->WindowSizeY)
						this->OnWindowResized(NewSizeX, NewSizeY);

					break;
				}

				}
			}

			}
		}

		// so scripts can use the `imgui_*` APIs
		// 09/11/2024
		{
			PROFILE_SCOPE("DearImGuiNewFrame");
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();
		}
		float aspectRatio = (float)this->WindowSizeX / (float)this->WindowSizeY;

		Object_Camera* sceneCamera = this->Workspace->GetSceneCamera();

		std::vector<Object_Base3D*> physicsList;

		PROFILE_EXPRESSION("UpdateScripts", updateScripts(deltaTime));

		// Aggregate mesh and light data into lists
		PROFILE_EXPRESSION(
			"RecurseDataModel",
			recursivelyTravelHierarchy(
				scene.RenderList,
				scene.LightingList,
				physicsList,
				this->Workspace,
				sceneCamera,
				deltaTime
			)
		);

		bool hasPhysics = false;

		for (Object_Base3D* object : physicsList)
			if (object->PhysicsDynamics)
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

		scene.UsedShaders = {};

		MaterialManager* mtlManager = MaterialManager::Get();

		for (const RenderItem& ri : scene.RenderList)
			scene.UsedShaders.insert(mtlManager->GetMaterialResource(ri.MaterialId).ShaderId);

		if (hasSun)
		{
			PROFILE_SCOPE("Shadows");

			Scene sunScene{};
			sunScene.RenderList.reserve(scene.RenderList.size());
			sunScene.UsedShaders = scene.UsedShaders;

			for (const RenderItem& ri : scene.RenderList)
				if (ri.CastsShadows)
					sunScene.RenderList.push_back(ri);

			glm::mat4 sunOrtho = glm::ortho(-35.0f, 35.0f, -35.0f, 35.0f, 0.1f, 75.0f);
			glm::mat4 sunView = glm::lookAt(50.f * sunDirection, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
			glm::mat4 sunRenderMatrix = sunOrtho * sunView;

			sunShadowMap.Bind();
			glViewport(0, 0, SunShadowMapResolutionSq, SunShadowMapResolutionSq);
			glClearColor(1.f, 1.f, 1.f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

		glm::vec3 camPos = glm::vec3(sceneCamera->Transform[3]);
		glm::vec3 camForward = glm::vec3(sceneCamera->Transform[2]);
		glm::vec3 camUp = glm::vec3(sceneCamera->Transform[1]);

		glm::mat4 view = view = glm::lookAt(camPos, camPos + camForward, camUp);
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

		skyboxShaders.SetUniform("RenderMatrix", projection * view);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);

		RendererContext.FrameBuffer.Bind();

		glClearColor(0.086f, 0.105f, 0.21f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		MeshProvider* mp = MeshProvider::Get();

		glDepthFunc(GL_LEQUAL);

		RendererContext.DrawMesh(
			mp->GetMeshResource(mp->LoadFromPath("!Cube")),
			skyboxShaders,
			Vector3::one,
			projection * view,
			FaceCullingMode::FrontFace // Cull the Outside, not the Inside
		);

		glDepthFunc(GL_LESS);

		//Main render pass
		RendererContext.DrawScene(scene, renderMatrix, sceneCamera->Transform, this->RunningTime);

		glDisable(GL_DEPTH_TEST);

		uint32_t dataModelId = this->DataModel->ObjectId;

		PROFILE_EXPRESSION("EventCallbacks/OnFrameRenderGui", this->OnFrameRenderGui.Fire(deltaTime));

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (!GameObject::GetObjectById(dataModelId))
		{
			Log::Error("DataModel was Destroy'd, shutting down...");
			this->DataModel = nullptr;
			GameObject::s_DataModel = nullptr;

			break;
		}

		glEnable(GL_DEPTH_TEST);

		//Do framebuffer stuff after everything is drawn

		RendererContext.FrameBuffer.Unbind();

		if (EngineJsonConfig.value("postfx_enabled", false))
		{
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

			postFxShaders.SetUniform("Time", static_cast<float>(this->RunningTime));

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, texManager->GetTextureResource(distortionTexture).GpuId);
		}
		else
		{
			postFxShaders.SetUniform("PostFxEnabled", 0);
			skyboxShaders.SetUniform(
				"HdrEnabled",
				false
			);
		}

		glActiveTexture(GL_TEXTURE1);
		RendererContext.FrameBuffer.BindTexture();

		glGenerateMipmap(GL_TEXTURE_2D);

		glDisable(GL_DEPTH_TEST);

		Profiler::Start("PostProcessing");

		RendererContext.DrawMesh(
			mp->GetMeshResource(mp->LoadFromPath("!Quad")),
			postFxShaders,
			Vector3::one*2.f,
			glm::mat4(1.f),
			FaceCullingMode::BackFace,
			0
		);

		Profiler::Stop();

		glEnable(GL_DEPTH_TEST);

		// End of frame

		this->RunningTime = GetRunningTime();

		double curTimePrevSwap = RunningTime;

		PROFILE_PROCEDURE("SwapBuffers", RendererContext.SwapBuffers);

		this->RunningTime = GetRunningTime();

		// Should be calculated before swapping buffers due to VSync
		this->FrameTime = curTimePrevSwap - FrameStart;
		LastFrame = RunningTime;

		m_DrawnFramesInSecond++;

		PROFILE_EXPRESSION("EventCallbacks/OnFrameEnd", OnFrameEnd.Fire(deltaTime));

		if (RunningTime - LastSecond > 1.0f)
		{
			LastSecond = RunningTime;

			this->FramesPerSecond = m_DrawnFramesInSecond;

			m_DrawnFramesInSecond = -1;

			Log::Save();
		}

		Profiler::Stop();

		Profiler::PushSnapshot();
	}

	Log::Info("Main loop exited");
}

EngineObject::~EngineObject()
{
	Log::Info("Engine destructing...");

	MaterialManager::Shutdown();
	TextureManager::Shutdown();
	ShaderManager::Shutdown();
	MeshProvider::Shutdown();

	// TODO:
	// 27/08/2024:
	// Even if I call the destructors here and clear the vector as well,
	// C++ *still* calls them again at program exit as the scope terminates.
	// It doesn't cause a use-after-free, YET
	this->DataModel->Destroy();
	GameObject::s_DataModel = nullptr;
	this->Workspace = nullptr;
	this->DataModel = nullptr;

	EngineInstance = nullptr;
}
