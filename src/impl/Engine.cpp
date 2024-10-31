#include <chrono>
#include <string>
#include <format>
#include <SDL2/SDL.h>
#include <glad/gl.h>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui/backends/imgui_impl_sdl2.h>

#include "Engine.hpp"

#include "gameobject/GameObjects.hpp"
#include "asset/TextureManager.hpp"
#include "asset/MeshProvider.hpp"
#include "GlobalJsonConfig.hpp"
#include "ThreadManager.hpp"
#include "UserInput.hpp"
#include "FileRW.hpp"
#include "Debug.hpp"

static uint32_t RectangleVAO, RectangleVBO;
static auto ChronoStartTime = std::chrono::high_resolution_clock::now();

// TODO: move 13/08/2024
static int WindowSizeXBeforeFullscreen = 800;
static int WindowSizeYBeforeFullscreen = 800;
static int WindowPosXBeforeFullscreen = SDL_WINDOWPOS_CENTERED;
static int WindowPosYBeforeFullscreen = SDL_WINDOWPOS_CENTERED;

static float RectangleVertices[24] =
{
		 1.0f, -1.0f,    1.0f, 0.0f,
		-1.0f, -1.0f,    0.0f, 0.0f,
		-1.0f,  1.0f,    0.0f, 1.0f,

		 1.0f,  1.0f,    1.0f, 1.0f,
		 1.0f, -1.0f,    1.0f, 0.0f,
		-1.0f,  1.0f,    0.0f, 1.0f
};

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
		Debug::Log(logString);
	else
		throw(logString);
}

static void updateDescendants(GameObject* Root, double DeltaTime)
{
	for (GameObject* object : Root->GetChildren())
	{
		if (object->Enabled)
		{
			object->Update(DeltaTime);

			if (!object->GetChildren().empty())
				updateDescendants(object, DeltaTime);
		}
	}
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

	RendererContext->ChangeResolution(WindowSizeX, WindowSizeY);
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

	Debug::Log("Configuration loaded");
}

EngineObject::EngineObject()
{
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
		Debug::Log(std::vformat(
			"Invalid/unsupported OpenGL profile '{}' requested, defaulting to the Core profile",
			std::make_format_args(requestedProfileString)
		));
	else
		requestedProfile = requestedProfileIt->second;

	Debug::Log(std::vformat(
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

	Debug::Log("Window created");

	// TODO: Engine->MSAASamples does nothing, attempting to specify via below ctor's argument leads to
	// OpenGL error "Target doesn't match the texture's target"
	this->RendererContext = new Renderer(this->WindowSizeX, this->WindowSizeY, this->Window);

	Debug::Log("Creating initial DataModel...");

	GameObject* newDataModel = GameObject::Create("DataModel");

	this->DataModel = dynamic_cast<Object_DataModel*>(newDataModel);
	GameObject::s_DataModel = newDataModel;

	GameObject* workspace = DataModel->GetChild("Workspace");
	this->Workspace = dynamic_cast<Object_Workspace*>(workspace);

	//ThreadManager::Get()->CreateWorkers(4, WorkerType::DefaultTaskWorker);

	Debug::Log("Engine constructed");
}

static void recursivelyTravelHierarchy(
	std::vector<RenderItem>& RenderList,
	std::vector<LightItem>& LightList,
	std::vector<Object_Base3D*>& PhysicsList,
	GameObject* Root,
	Object_Camera* SceneCamera
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

		Object_Base3D* object3D = dynamic_cast<Object_Base3D*>(object);

		if (object3D)
		{
			if (object3D->PhysicsDynamics || object3D->PhysicsCollisions)
				PhysicsList.emplace_back(object3D);

			if (object3D->Transparency > .95f)
				continue;

			// TODO: frustum culling
			// Hold R to disable distance culling
			if (glm::distance(
				glm::vec3(object3D->Transform[3]),
				glm::vec3(SceneCamera->Transform[3])
				) > 100.0f
				&& !UserInput::IsKeyDown(SDLK_r)
			)
				continue;

			//TODO: recheck whether we need this
			// if (MeshObject->HasTransparency)
				//continue;

			RenderList.emplace_back(
				object3D->GetRenderMeshId(),
				object3D->Transform,
				object3D->Size,
				object3D->Material,
				object3D->ColorRGB,
				object3D->Transparency,
				object3D->Reflectivity,
				object3D->FaceCulling
			);
		}

		Object_Light* light = !object3D ? dynamic_cast<Object_Light*>(object) : nullptr;

		if (light)
		{
			Object_DirectionalLight* directional = dynamic_cast<Object_DirectionalLight*>(light);
			Object_PointLight* point = dynamic_cast<Object_PointLight*>(light);

			if (directional)
				LightList.emplace_back(
					LightType::Directional,
					(glm::vec3)light->LocalTransform[3],
					light->LightColor * light->Brightness
				);

			if (point)
				LightList.emplace_back(
					LightType::Point,
					(glm::vec3)light->GetWorldTransform()[3],
					light->LightColor * light->Brightness,
					point->Range
				);
		}

		if (!object->GetChildren().empty())
			recursivelyTravelHierarchy(
				RenderList,
				LightList,
				PhysicsList,
				object,
				SceneCamera
			);
	}
}

static double GetRunningTime()
{
	auto chronoTime = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::nanoseconds>(chronoTime - ChronoStartTime).count() / 1e+9;
}

void EngineObject::Start()
{
	Debug::Log("Final initializations...");

	// Post-processing framebuffer quad
	glGenVertexArrays(1, &RectangleVAO);
	glGenBuffers(1, &RectangleVBO);
	glBindVertexArray(RectangleVAO);
	glBindBuffer(GL_ARRAY_BUFFER, RectangleVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(RectangleVertices), &RectangleVertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

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
		"bottom",
		"top",
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

	for (const std::string& skyboxImage : SkyboxCubemapImages)
	{
		uint32_t tex = texManager->LoadTextureFromPath(SkyPath + skyboxImage + ".jpg");
		skyboxFacesBeingLoaded.push_back(tex);
	}

	//Texture HDRISkyboxTexture("Assets/Textures/hdri_sky_1.jpeg", "Diffuse", 0);

	ShaderProgram* postFxShaders = ShaderProgram::GetShaderProgram("postprocessing");
	ShaderProgram* skyboxShaders = ShaderProgram::GetShaderProgram("skybox");

	postFxShaders->SetUniform("Texture", 1);
	postFxShaders->SetUniform("DistortionTexture", 2);

	skyboxShaders->SetUniform("SkyboxCubemap", 3);

	Scene scene = Scene();

	RendererContext->Framebuffer->Unbind();
	
	uint32_t distortionTexture = texManager->LoadTextureFromPath("textures/screendistort.jpg", false);

	SDL_Event pollingEvent;

	Debug::Log("Main engine loop start");

	while (!this->Exit)
	{
		if (this->DataModel->WantExit)
		{
			Debug::Log("DataModel requested shutdown");
			break;
		}

		if (!DataModel->GetChildOfClass("Workspace"))
		{
			Debug::Log("Workspace was removed, shutting down");
			break;
		}

		this->RunningTime = GetRunningTime();

		scene.RenderList.clear();
		scene.LightingList.clear();

		TextureManager::Get()->FinalizeAsyncLoadedTextures();
		MeshProvider::Get()->FinalizeAsyncLoadedMeshes();

		bool skyboxLoaded = true;

		for (uint32_t skyboxFace : skyboxFacesBeingLoaded)
		{
			Texture* texture = texManager->GetTextureResource(skyboxFace);

			if (texture->Status != Texture::LoadStatus::Succeeded)
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
				Texture* texture = texManager->GetTextureResource(skyboxFacesBeingLoaded.at(skyboxFaceIndex));

				glTexImage2D(
					GL_TEXTURE_CUBE_MAP_POSITIVE_X + skyboxFaceIndex,
					0,
					GL_RGB,
					texture->Width,
					texture->Height,
					0,
					GL_RGB,
					GL_UNSIGNED_BYTE,
					texture->TMP_ImageByteData
				);

				glGenerateMipmap(GL_TEXTURE_2D);

				free(texture->TMP_ImageByteData);
				texture->TMP_ImageByteData = nullptr;
			}

			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

			skyboxFacesBeingLoaded.clear();
		}

		this->FpsCap = std::clamp(this->FpsCap, 1, 600);

		double frameDelta = RunningTime - LastFrame;
		double fpsCapDelta = 1.f / this->FpsCap;

		// Wait the appropriate amount of time between frames
		if (!VSync && (frameDelta < fpsCapDelta))
		{
			SDL_Delay(static_cast<uint32_t>((fpsCapDelta - frameDelta) * 1000));
			continue;
		}

		double deltaTime = GetRunningTime() - LastTime;
		LastTime = RunningTime;
		FrameStart = RunningTime;

		this->OnFrameStart.Fire(deltaTime);

		while (SDL_PollEvent(&pollingEvent) != 0)
		{
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

		DataModel->Update(deltaTime);
		updateDescendants(DataModel, deltaTime);

		float aspectRatio = (float)this->WindowSizeX / (float)this->WindowSizeY;

		GameObject* workspace = dynamic_cast<GameObject*>(Workspace);
		Object_Camera* sceneCamera = this->Workspace->GetSceneCamera();

		glm::mat4 cameraMatrix = sceneCamera->GetMatrixForAspectRatio(aspectRatio);
		
		std::vector<Object_Base3D*> physicsList;

		// Aggregate mesh and light data into lists
		recursivelyTravelHierarchy(
			scene.RenderList,
			scene.LightingList,
			physicsList,
			workspace,
			sceneCamera
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

		// TODO 15/09/2024
		// Move into the Renderer. Can't right now because it isn't aware of the `CameraMatrix`
		// or `Time`.
		
		// Hashmap better than linaer serch
		
		scene.UsedShaders = {};

		for (const RenderItem& md : scene.RenderList)
			scene.UsedShaders.insert(md.Material->Shader);

		for (ShaderProgram* shp : scene.UsedShaders)
		{
			shp->SetUniform("CameraMatrix", cameraMatrix);
			shp->SetUniform("CameraPosition", Vector3(glm::vec3(sceneCamera->Transform[3])).ToGenericValue());
			shp->SetUniform("Time", static_cast<float>(this->RunningTime));
			shp->SetUniform("SkyboxCubemap", 3);
		}

		glActiveTexture(GL_TEXTURE3);

		glDepthFunc(GL_LEQUAL);

		glm::vec3 camPos = glm::vec3(sceneCamera->Transform[3]);
		glm::vec3 camForward = glm::vec3(sceneCamera->Transform[2]);

		glm::mat4 view = view = glm::lookAt(camPos, camPos + camForward, glm::vec3(0.f, 1.f, 0.f));;
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
		// Wow Mr Victor Gordan sir, that sounds incredibly overcomplicated.
		// It's really too bad there isn't a way simpler, 300x more understandable way
		// of zeroing-out the first 3 values of the last column of what is in essence a 4x4 2D array of floats...
		view[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);

		skyboxShaders->SetUniform("CameraMatrix", projection * view);

		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);
		
		RendererContext->Framebuffer->Bind();

		glClearColor(0.086f, 0.105f, 0.21f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		MeshProvider* mp = MeshProvider::Get();

		RendererContext->DrawMesh(
			mp->GetMeshResource(mp->LoadFromPath("!Cube")),
			skyboxShaders,
			Vector3::one,
			projection * view,
			FaceCullingMode::FrontFace // Cull the Outside, not the Inside
		);

		glDepthFunc(GL_LESS);

		//Main render pass
		RendererContext->DrawScene(scene);

		glDisable(GL_DEPTH_TEST);

		uint32_t dataModelId = this->DataModel->ObjectId;

		this->OnFrameRenderGui.Fire(deltaTime);

		if (!GameObject::GetObjectById(dataModelId))
		{
			Debug::Log("DataModel was Destroy'd, shutting down...");
			this->DataModel = nullptr;
			GameObject::s_DataModel = nullptr;

			break;
		}

		glEnable(GL_DEPTH_TEST);

		//Do framebuffer stuff after everything is drawn

		RendererContext->Framebuffer->Unbind();

		if (EngineJsonConfig.value("postfx_enabled", false))
		{
			postFxShaders->SetUniform("PostFxEnabled", 1);
			postFxShaders->SetUniform(
				"ScreenEdgeBlurEnabled",
				EngineJsonConfig.value("postfx_blurvignette", false)
			);
			postFxShaders->SetUniform(
				"DistortionEnabled",
				EngineJsonConfig.value("postfx_distortion", false)
			);

			postFxShaders->SetUniform(
				"Gamma",
				EngineJsonConfig.value("postfx_gamma", 1.f)
			);
			postFxShaders->SetUniform(
				"LdMax",
				EngineJsonConfig.value("postfx_ldmax", 1.f)
			);
			postFxShaders->SetUniform(
				"ContrastMax",
				EngineJsonConfig.value("postfx_cmax", 1.f)
			);
			skyboxShaders->SetUniform(
				"HdrEnabled",
				true
			);

			if (EngineJsonConfig.find("postfx_blurvignette_blurstrength") != EngineJsonConfig.end())
			{
				postFxShaders->SetUniform(
					"BlurVignetteStrength",
					(float)EngineJsonConfig["postfx_blurvignette_blurstrength"]
				);
				postFxShaders->SetUniform(
					"BlurVignetteDistMul",
					(float)EngineJsonConfig["postfx_blurvignette_weightmul"]
				);
				postFxShaders->SetUniform(
					"BlurVignetteDistExp",
					(float)EngineJsonConfig["postfx_blurvignette_weightexp"]
				);
				postFxShaders->SetUniform(
					"BlurVignetteSampleRadius",
					(int)EngineJsonConfig["postfx_blurvignette_sampleradius"]
				);
			}

			postFxShaders->SetUniform("Time", static_cast<float>(this->RunningTime));

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, texManager->GetTextureResource(distortionTexture)->GpuId);
		}
		else
		{
			postFxShaders->SetUniform("PostFxEnabled", 0);
			skyboxShaders->SetUniform(
				"HdrEnabled",
				false
			);
		}

		glActiveTexture(GL_TEXTURE1);
		RendererContext->Framebuffer->BindTexture();

		glGenerateMipmap(GL_TEXTURE_2D);

		glBindVertexArray(RectangleVAO);
		glDisable(GL_DEPTH_TEST);

		postFxShaders->Activate();

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glEnable(GL_DEPTH_TEST);

		// End of frame

		this->RunningTime = GetRunningTime();

		double curTimePrevSwap = RunningTime;

		RendererContext->SwapBuffers();

		this->RunningTime = GetRunningTime();

		// Should be calculated before swapping buffers due to VSync
		this->FrameTime = curTimePrevSwap - FrameStart;
		LastFrame = RunningTime;

		m_DrawnFramesInSecond++;

		this->OnFrameEnd.Fire(deltaTime);

		if (RunningTime - LastSecond > 1.0f)
		{
			LastSecond = RunningTime;

			this->FramesPerSecond = m_DrawnFramesInSecond;

			m_DrawnFramesInSecond = -1;

			Debug::Save();
		}
	}

	Debug::Log("Main loop exited");
}

EngineObject::~EngineObject()
{
	Debug::Log("Engine destructing...");

	ShaderProgram::ClearAll();

	MeshProvider::Shutdown();
	TextureManager::Shutdown();
	delete this->RendererContext;

	// TODO:
	// 27/08/2024:
	// Even if I call the destructors here and clear the vector as well,
	// C++ *still* calls them again at program exit as the scope terminates.
	// It doesn't cause a use-after-free, YET
	delete this->DataModel;
	GameObject::s_DataModel = nullptr;
	this->Workspace = nullptr;
	this->DataModel = nullptr;

	SDL_DestroyWindow(this->Window);
	SDL_Quit();
}
