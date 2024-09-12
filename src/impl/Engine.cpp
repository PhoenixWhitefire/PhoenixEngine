#include<chrono>
#include<string>
#include<format>
#include<SDL2/SDL.h>
#include<glad/gl.h>
#include<stb_image.h>
#include<glm/matrix.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<imgui/backends/imgui_impl_sdl2.h>

#include"Engine.hpp"

#include"gameobject/GameObjects.hpp"
#include"GlobalJsonConfig.hpp"
#include"ThreadManager.hpp"
#include"UserInput.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"
#include"BaseMeshes.hpp"

static uint32_t RectangleVAO, RectangleVBO;
static auto ChronoStartTime = std::chrono::high_resolution_clock::now();

// TODO: move 13/08/2024
static int WindowSizeXBeforeFullscreen = 800;
static int WindowSizeYBeforeFullscreen = 800;
static int WindowPosXBeforeFullscreen = SDL_WINDOWPOS_CENTERED;
static int WindowPosYBeforeFullscreen = SDL_WINDOWPOS_CENTERED;

static float RectangleVertices[24] = {
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
		object->Update(DeltaTime);

		if (object->GetChildren().size() > 0)
			updateDescendants(object, DeltaTime);
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
	// `GameObject::CreateGameObject` today.
	// My idea was, "Since constructors return objects, why don't I
	// just have a list of constructors"?
	// I didn't really think it would work initially, because it wouldn't
	// even be returning the base class and thus there couldn't be an assignable
	// type, but I didn't expect the _actual_ error to be:
	// :cloud_with_lightning: "The pointer to a constructor shalt never be taken" :high_voltage:
	// ... or something along those lines.
	// 
	//GameObject::GameObjectTable["Model"] = &Object_Model()

	this->LoadConfiguration();

	nlohmann::json defaultWindowSize = EngineJsonConfig.value(
		"DefaultWindowSize",
		nlohmann::json::array({ 1280, 720 })
	);

	this->WindowSizeX = defaultWindowSize[0], this->WindowSizeY = defaultWindowSize[1];

	PHX_SDL_CALL(SDL_Init, SDL_INIT_VIDEO);

	Debug::Log("Initialized SDL Video subsystem");

	// This is easily the worst complaint I've had about this library,
	// the log function *does not called even when an error retrievable by SDL_GetError occurs*!
	// It's complete RUBBISH, USELESS, TRASH, BULLSHIT
	// 09/09/2024
	SDL_LogSetOutputFunction(sdlLog, nullptr);

	// Must be set *before* window creation
	PHX_SDL_CALL(
		SDL_GL_SetAttribute,
		SDL_GL_CONTEXT_PROFILE_MASK, 
		SDL_GL_CONTEXT_PROFILE_CORE
	);
	PHX_SDL_CALL(
		SDL_GL_SetAttribute,
		SDL_GL_CONTEXT_MAJOR_VERSION, 
		EngineJsonConfig.value("SDL_GL_VersionMajor", 4)
	);
	PHX_SDL_CALL(
		SDL_GL_SetAttribute,
		SDL_GL_CONTEXT_MINOR_VERSION, 
		EngineJsonConfig.value("SDL_GL_VersionMinor", 6)
	);

	PHX_SDL_CALL(SDL_GL_SetAttribute, SDL_GL_DOUBLEBUFFER, 1);
	PHX_SDL_CALL(SDL_GL_SetAttribute, SDL_GL_DEPTH_SIZE, 24);
	PHX_SDL_CALL(SDL_GL_SetAttribute, SDL_GL_STENCIL_SIZE, 8);

	this->Window = SDL_CreateWindow(
		"Waiting on configuration...",
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

	SDL_SetWindowTitle(this->Window, EngineJsonConfig.value("GameTitle", "PhoenixEngine").c_str());

	Debug::Log("Window configured");

	// TODO: Engine->MSAASamples does nothing, attempting to specify via below ctor's argument leads to
	// OpenGL error "Target doesn't match the texture's target"
	this->RendererContext = new Renderer(this->WindowSizeX, this->WindowSizeY, this->Window);

	Debug::Log("Creating datamodel...");

	GameObject* newDataModel = GameObject::CreateGameObject("DataModel");

	this->DataModel = dynamic_cast<Object_DataModel*>(newDataModel);
	GameObject::s_DataModel = newDataModel;

	GameObject* workspace = DataModel->GetChild("Workspace");

	this->Workspace = dynamic_cast<Object_Workspace*>(workspace);

	// Post-processing framebuffer

	glGenVertexArrays(1, &RectangleVAO);
	glGenBuffers(1, &RectangleVBO);
	glBindVertexArray(RectangleVAO);
	glBindBuffer(GL_ARRAY_BUFFER, RectangleVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(RectangleVertices), &RectangleVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	ThreadManager::Get()->CreateWorkers(4, WorkerType::DefaultTaskWorker);

	Debug::Log("Engine constructed");
}

static std::vector<LightItem> createLightingList(GameObject* RootObject)
{
	std::vector<GameObject*> objects = RootObject->GetChildren();
	std::vector<LightItem> dataList;

	for (GameObject* object : objects)
	{
		if (!object->Enabled)
			continue;

		Object_Light* light = dynamic_cast<Object_Light*>(object);

		if (light)
		{
			Object_DirectionalLight* directional = dynamic_cast<Object_DirectionalLight*>(light);
			Object_PointLight* point = dynamic_cast<Object_PointLight*>(light);

			LightItem data;

			if (directional)
			{
				data.Position = light->Position;
				data.LightColor = light->LightColor * light->Brightness;
				data.Type = LightType::Directional;
			}

			if (point)
			{
				data.Position = light->Position;
				data.LightColor = light->LightColor * light->Brightness;
				data.Type = LightType::Point;
				data.Range = point->Range;
			}

			dataList.push_back(data);
		}

		if (object->GetChildren().size() > 0)
		{
			std::vector<LightItem> childData = createLightingList(object);
			std::copy(childData.begin(), childData.end(), std::back_inserter(dataList));
		}
	}

	return dataList;
}

static std::vector<RenderItem> createRenderList(GameObject* RootObject, Object_Camera* SceneCamera)
{
	std::vector<GameObject*> objects = RootObject->GetChildren();
	std::vector<RenderItem> dataList;

	for (GameObject* object : objects)
	{
		if (object->Enabled)
		{
			Object_Base3D* object3D = dynamic_cast<Object_Base3D*>(object);

			if (object3D)
			{
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

				RenderItem data
				{
					object3D->GetRenderMesh(),
					object3D->Transform,
					object3D->Size,
					object3D->Material,
					object3D->ColorRGB,
					object3D->Transparency,
					object3D->Reflectivity,
					object3D->FaceCulling
				};

				dataList.push_back(data);
			}

			if (object->GetChildren().size() > 0)
			{
				std::vector<RenderItem> childData = createRenderList(object, SceneCamera);
				std::copy(childData.begin(), childData.end(), std::back_inserter(dataList));
			}
		}
	}

	return dataList;
}

static double GetRunningTime()
{
	auto chronoTime = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::nanoseconds>(chronoTime - ChronoStartTime).count() / 1e+9;
}

void EngineObject::Start()
{
	Debug::Log("Final initializations...");

	// TODO:
	// wtf are these
	// 13/07/2024
	double LastTime = this->RunningTime;
	double LastFrame = GetRunningTime();
	double FrameStart = 0.0f;
	double LastSecond = LastFrame;

	this->Exit = false;

	const std::string SkyPath = "textures/Sky1/";

	static const std::string SkyboxCubemapImages[6] =
	{
		"right",
		"left",
		"bottom",
		"top",
		"front",
		"back"
	};

	std::vector<Texture*> skyboxFacesLoading;

	GLuint skyboxCubemap = 0;

	glGenTextures(1, &skyboxCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	//glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_LOD_BIAS, 15.f);

	for (const std::string skyboxFace : SkyboxCubemapImages)
	{
		Texture* tex = TextureManager::Get()->LoadTextureFromPath(SkyPath + skyboxFace + ".jpg");
		skyboxFacesLoading.push_back(tex);
	}

	GLubyte whitePixel = 0xFF;

	for (int imageIndex = 0; imageIndex < 6; imageIndex++)
	{
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + imageIndex,
			0,
			GL_RED,
			1,
			1,
			0,
			GL_RED,
			GL_UNSIGNED_BYTE,
			(void*)&whitePixel
		);

		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	//Texture HDRISkyboxTexture("Assets/Textures/hdri_sky_1.jpeg", "Diffuse", 0);

	ShaderProgram* postFxShaders = ShaderProgram::GetShaderProgram("postprocessing");
	ShaderProgram* skyboxShaders = ShaderProgram::GetShaderProgram("skybox");

	postFxShaders->SetUniformInt("Texture", 1);
	postFxShaders->SetUniformInt("DistortionTexture", 2);

	skyboxShaders->SetUniformInt("SkyCubemap", 3);

	Scene scene = Scene();

	RendererContext->Framebuffer->Unbind();

	Texture* distortionTexture = TextureManager::Get()->LoadTextureFromPath("textures/screendistort.jpg");

	SDL_Event pollingEvent;

	Debug::Log("Main program loop start...");

	while (!this->Exit)
	{
		if (this->DataModel->WantExit)
		{
			Debug::Log("DataModel requested shutdown");
			break;
		}

		if (!DataModel->GetChildOfClass("Workspace"))
		{
			Debug::Log("Workspace was Destroyed, shutting down");
			break;
		}

		this->RunningTime = GetRunningTime();

		scene.RenderList.clear();
		scene.LightingList.clear();

		TextureManager::Get()->FinalizeAsyncLoadedTextures();

		bool skyboxLoaded = true;

		for (int index = 0; index < skyboxFacesLoading.size(); index++)
		{
			Texture* face = skyboxFacesLoading[index];

			if (face->AttemptedLoad && face->Identifier != TEXTUREMANAGER_INVALID_ID)
			{
				//glDeleteTextures(1, &face->Identifier);

				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);

				glTexImage2D(
					GL_TEXTURE_CUBE_MAP_POSITIVE_X + index,
					0,
					GL_RGB,
					face->Width,
					face->Height,
					0,
					GL_RGB,
					GL_UNSIGNED_BYTE,
					face->TMP_ImageByteData
				);
			}
			else
			{
				skyboxLoaded = false;
				break;
			}
		}

		if (skyboxLoaded && skyboxFacesLoading.size() > 0)
		{
			skyboxFacesLoading.clear();
			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
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

		double deltaTime = RunningTime - LastTime;
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
		updateDescendants(dynamic_cast<GameObject*>(DataModel), deltaTime);

		double aspectRatio = (double)this->WindowSizeX / (double)this->WindowSizeY;

		GameObject* workspace = dynamic_cast<GameObject*>(Workspace);
		Object_Camera* sceneCamera = this->Workspace->GetSceneCamera();

		glm::mat4 cameraMatrix = sceneCamera->GetMatrixForAspectRatio(aspectRatio);

		// Aggregate mesh and light data into a list
		scene.RenderList = createRenderList(workspace, sceneCamera);
		scene.LightingList = createLightingList(workspace);

		// Hashmap better than linaer serch
		std::unordered_map<ShaderProgram*, ShaderProgram*> uniqueShaderMap;

		for (RenderItem md : scene.RenderList)
		{
			if (uniqueShaderMap.find(md.Material->Shader) == uniqueShaderMap.end())
				uniqueShaderMap.insert(std::pair(md.Material->Shader, md.Material->Shader));
		}

		for (auto& it : uniqueShaderMap)
		{
			ShaderProgram* shp = it.second;

			shp->SetUniformMatrix("CameraMatrix", cameraMatrix);
			shp->SetUniformFloat("Time", static_cast<float>(this->RunningTime));

			scene.UniqueShaders.push_back(shp);
		}

		glActiveTexture(GL_TEXTURE3);

		glDepthFunc(GL_LEQUAL);

		glm::vec3 camPos = glm::vec3(sceneCamera->Transform[3]);
		glm::vec3 camForward = glm::vec3(sceneCamera->Transform[2]);

		glm::mat4 view = glm::mat4(1.f);
		glm::mat4 projection = glm::mat4(1.0f);

		// We make the mat4 into a mat3 and then a mat4 again in order to get rid of the last row and column
		// The last row and column affect the translation of the skybox (which we don't want to affect)

		view = glm::mat4(glm::mat3(glm::lookAt(camPos, camPos + camForward, glm::vec3(0.f, 1.f, 0.f))));

		projection = glm::perspective(
			glm::radians(sceneCamera->FieldOfView),
			aspectRatio,
			sceneCamera->NearPlane,
			sceneCamera->FarPlane
		);

		skyboxShaders->SetUniformMatrix("CameraMatrix", projection * view);

		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);
		
		RendererContext->Framebuffer->Bind();

		glClearColor(0.086f, 0.105f, 0.21f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		RendererContext->DrawMesh(
			BaseMeshes::Cube(),
			skyboxShaders,
			Vector3::one,
			projection * view,
			FaceCullingMode::FrontFace // Cull the Outside, not the Inside
		);

		glDepthFunc(GL_LESS);

		//Main render pass
		RendererContext->DrawScene(scene);

		for (GameObject* object : this->Workspace->GetDescendants())
		{
			Object_ParticleEmitter* particleEmitter = dynamic_cast<Object_ParticleEmitter*>(object);

			if (particleEmitter)
				particleEmitter->Render(sceneCamera->Transform);
		}

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
			postFxShaders->SetUniformInt("PostFxEnabled", 1);
			postFxShaders->SetUniformInt(
				"ScreenEdgeBlurEnabled",
				EngineJsonConfig.value("postfx_blurvignette", false)
			);
			postFxShaders->SetUniformInt(
				"DistortionEnabled",
				EngineJsonConfig.value("postfx_distortion", false)
			);

			if (EngineJsonConfig.find("postfx_blurvignette_blurstrength") != EngineJsonConfig.end())
			{
				postFxShaders->SetUniformFloat(
					"BlurVignetteStrength",
					EngineJsonConfig["postfx_blurvignette_blurstrength"]
				);
				postFxShaders->SetUniformFloat(
					"BlurVignetteDistMul",
					EngineJsonConfig["postfx_blurvignette_weightmul"]
				);
				postFxShaders->SetUniformFloat(
					"BlurVignetteDistExp",
					EngineJsonConfig["postfx_blurvignette_weightexp"]
				);
				postFxShaders->SetUniformInt(
					"BlurVignetteSampleRadius",
					EngineJsonConfig["postfx_blurvignette_sampleradius"]
				);
			}

			postFxShaders->SetUniformFloat("Time", static_cast<float>(this->RunningTime));

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, distortionTexture->Identifier);
		}
		else
			postFxShaders->SetUniformInt("PostFxEnabled", 0);

		glActiveTexture(GL_TEXTURE1);
		RendererContext->Framebuffer->BindTexture();

		glBindVertexArray(RectangleVAO);
		glDisable(GL_DEPTH_TEST);

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
