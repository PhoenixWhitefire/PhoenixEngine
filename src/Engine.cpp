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

static std::vector<Vertex> SkyboxVertices =
{
	/*
	//   Coordinates
	-1.0f, -1.0f,  1.0f,//        7--------6
	 1.0f, -1.0f,  1.0f,//       /|       /|
	 1.0f, -1.0f, -1.0f,//      4--------5 |
	-1.0f, -1.0f, -1.0f,//      | |      | |
	-1.0f,  1.0f,  1.0f,//      | 3------|-2
	 1.0f,  1.0f,  1.0f,//      |/       |/
	 1.0f,  1.0f, -1.0f,//      0--------1
	-1.0f,  1.0f, -1.0f,
	*/

	Vertex{ glm::vec3( -1.0f, -1.0f,  1.0f), glm::vec3(), glm::vec3(), glm::vec2() },
	Vertex{ glm::vec3(  1.0f, -1.0f,  1.0f), glm::vec3(), glm::vec3(), glm::vec2() },
	Vertex{ glm::vec3(  1.0f, -1.0f, -1.0f), glm::vec3(), glm::vec3(), glm::vec2() },
	Vertex{ glm::vec3( -1.0f, -1.0f, -1.0f), glm::vec3(), glm::vec3(), glm::vec2() },
	Vertex{ glm::vec3( -1.0f,  1.0f,  1.0f), glm::vec3(), glm::vec3(), glm::vec2() },
	Vertex{ glm::vec3(  1.0f,  1.0f,  1.0f), glm::vec3(), glm::vec3(), glm::vec2() },
	Vertex{ glm::vec3(  1.0f,  1.0f, -1.0f), glm::vec3(), glm::vec3(), glm::vec2() },
	Vertex{ glm::vec3( -1.0f,  1.0f, -1.0f), glm::vec3(), glm::vec3(), glm::vec2() },

};

static std::vector<uint32_t> SkyboxIndices =
{
	// Right
	1, 2, 6,
	6, 5, 1,
	// Left
	0, 4, 7,
	7, 3, 0,
	// Top
	4, 5, 6,
	6, 7, 4,
	// Bottom
	0, 3, 2,
	2, 1, 0,
	// Back
	0, 1, 5,
	5, 4, 0,
	// Front
	3, 7, 6,
	6, 2, 3
};

static void UpdateDescendants(GameObject* root, double DeltaTime)
{
	for (GameObject* obj : root->GetChildren())
	{
		obj->Update(DeltaTime);

		if (obj->GetChildren().size() > 0)
			UpdateDescendants(obj, DeltaTime);
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

	const int InitialWindowSizeX = 1280, InitialWindowSizeY = 720;
	this->WindowSizeX = InitialWindowSizeX, this->WindowSizeY = InitialWindowSizeY;

	SDL_Init(SDL_INIT_VIDEO);

	Debug::Log("Initialized SDL Video subsystem");
	
	// Must be set *before* window creation
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	Debug::Log("Set OpenGL window attributes");

	// Initialize SDL
	this->Window = SDL_CreateWindow(
		"Waiting on configuration...",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		InitialWindowSizeX, InitialWindowSizeY,
		m_SDLWindowFlags
	);

	Debug::Log("Window created");

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

	SDL_SetWindowSize(
		this->Window,
		EngineJsonConfig["DefaultWindowSize"][0],
		EngineJsonConfig["DefaultWindowSize"][1]
	);

	SDL_SetWindowTitle(this->Window, EngineJsonConfig.value("GameTitle", "PhoenixEngine").c_str());

	if (!this->Window)
	{
		const char* errStr = SDL_GetError();
		throw(std::vformat("SDL could not create the window: {}\n", std::make_format_args(errStr)));
	}

	Debug::Log("Window configured");

	// TODO: Engine->MSAASamples does nothing, attempting to specify via below ctor's argument leads to
	// OpenGL error "Target doesn't match the texture's target"
	this->RendererContext = new Renderer(this->WindowSizeX, this->WindowSizeY, this->Window);

	Debug::Log("Creating datamodel...");

	GameObject* newDataModel = GameObject::CreateGameObject("DataModel");

	this->DataModel = dynamic_cast<Object_DataModel*>(newDataModel);
	GameObject::s_DataModel = newDataModel;

	GameObject* WorkspaceShared = DataModel->GetChild("Workspace");

	if (!WorkspaceShared)
		throw("The Workspace was removed, or otherwise could not be found.");

	this->Workspace = dynamic_cast<Object_Workspace*>(WorkspaceShared);

	this->PostProcessingShaders = ShaderProgram::GetShaderProgram("postprocessing");

	this->PostProcessingShaders->Activate();
	glUniform1i(glGetUniformLocation(this->PostProcessingShaders->ID, "Texture"), 0);

	glEnable(GL_DEPTH_TEST);

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

static std::vector<LightData_t> compileLightData(GameObject* RootObject)
{
	std::vector<GameObject*> Objects = RootObject->GetChildren();
	std::vector<LightData_t> DataList;

	for (GameObject* Object : Objects)
	{
		if (!Object->Enabled)
			continue;

		Object_Light* LightObject = dynamic_cast<Object_Light*>(Object);

		if (LightObject)
		{
			Object_DirectionalLight* Directional = dynamic_cast<Object_DirectionalLight*>(LightObject);
			Object_PointLight* Point = dynamic_cast<Object_PointLight*>(LightObject);

			LightData_t Data;

			if (Directional)
			{
				Data.Position = LightObject->Position;
				Data.LightColor = LightObject->LightColor * LightObject->Brightness;
				Data.Type = LightType::DirectionalLight;
			}

			if (Point)
			{
				Data.Position = LightObject->Position;
				Data.LightColor = LightObject->LightColor * LightObject->Brightness;
				Data.Type = LightType::Pointlight;
				Data.Range = Point->Range;
			}

			DataList.push_back(Data);
		}

		if (Object->GetChildren().size() > 0)
		{
			std::vector<LightData_t> ChildData = compileLightData(Object);

			for (uint32_t CDataIdx = 0; CDataIdx < ChildData.size(); CDataIdx++)
				DataList.push_back(ChildData[CDataIdx]);
		}
	}

	return DataList;
}

static std::vector<MeshData_t> compileMeshData(GameObject* RootObject, Object_Camera* SceneCamera)
{
	std::vector<GameObject*> Objects = RootObject->GetChildren();
	std::vector<MeshData_t> DataList;

	for (GameObject* Object : Objects)
	{
		if (Object->Enabled)
		{
			Object_Base3D* Object3D = dynamic_cast<Object_Base3D*>(Object);

			if (Object3D != nullptr)
			{
				// TODO: frustum culling
				// Hold R to disable distance culling
				if (glm::distance(glm::vec3(Object3D->Transform[3]), glm::vec3(SceneCamera->Transform[3])) > 100.0f
					&& !UserInput::IsKeyDown(SDLK_r))
					continue;

				//TODO: recheck whether we need this
				// if (MeshObject->HasTransparency)
					//continue;

				MeshData_t Data
				{
					Object3D->GetRenderMesh(),
					Object3D->Transform,
					Object3D->Size,
					Object3D->Material,
					Object3D->ColorRGB,
					Object3D->Transparency,
					Object3D->Reflectivity,
					Object3D->FaceCulling
				};
				DataList.push_back(Data);
			}

			if (Object->GetChildren().size() > 0)
			{
				std::vector<MeshData_t> ChildData = compileMeshData(Object, SceneCamera);

				for (uint32_t CDataIdx = 0; CDataIdx < ChildData.size(); CDataIdx++)
					DataList.push_back(ChildData[CDataIdx]);
			}
		}
	}

	return DataList;
}

static double GetRunningTime()
{
	auto ChronoTime = std::chrono::high_resolution_clock::now();

	return std::chrono::duration_cast<std::chrono::nanoseconds>(ChronoTime - ChronoStartTime).count() / 1e+9;
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

	std::vector<Texture*> SkyboxFacesLoading;

	GLuint SkyboxCubemap = 0;

	glGenTextures(1, &SkyboxCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, SkyboxCubemap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	//glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_LOD_BIAS, 15.f);

	for (const std::string skyboxFace : SkyboxCubemapImages)
	{
		Texture* tx = TextureManager::Get()->LoadTextureFromPath(SkyPath + skyboxFace + ".jpg");
		SkyboxFacesLoading.push_back(tx);
	}

	GLubyte WhitePixel = 0xFF;

	for (int ImageIndex = 0; ImageIndex < 6; ImageIndex++)
	{
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + ImageIndex,
			0,
			GL_RED,
			1,
			1,
			0,
			GL_RED,
			GL_UNSIGNED_BYTE,
			(void*)&WhitePixel
		);

		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	//Texture HDRISkyboxTexture("Assets/Textures/hdri_sky_1.jpeg", "Diffuse", 0);

	ShaderProgram* SkyboxShaders = ShaderProgram::GetShaderProgram("skybox");

	SkyboxShaders->Activate();

	glUniform1i(glGetUniformLocation(SkyboxShaders->ID, "SkyCubemap"), 3);

	Scene_t Scene = Scene_t();

	RendererContext->Framebuffer->Unbind();

	Texture* DistortionTexture = TextureManager::Get()->LoadTextureFromPath("textures/screendistort.jpg");

	Mesh SkyboxMesh = Mesh(SkyboxVertices, SkyboxIndices);

	SDL_Event PollingEvent;

	Debug::Log("Main program loop start...");

	while (!(this->Exit || this->DataModel->WantExit))
	{
		if (!DataModel->GetChildOfClass("Workspace"))
		{
			Debug::Log("Workspace was Destroyed, shutting down");
			break;
		}

		this->RunningTime = GetRunningTime();

		Scene.MeshData.clear();
		Scene.LightData.clear();

		TextureManager::Get()->FinalizeAsyncLoadedTextures();

		bool skyboxLoaded = true;

		for (int idx = 0; idx < SkyboxFacesLoading.size(); idx++)
		{
			Texture* face = SkyboxFacesLoading[idx];

			if (face->AttemptedLoad && face->Identifier != TEXTUREMANAGER_INVALID_ID)
			{
				//glDeleteTextures(1, &face->Identifier);

				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_CUBE_MAP, SkyboxCubemap);

				glTexImage2D(
					GL_TEXTURE_CUBE_MAP_POSITIVE_X + idx,
					0,
					GL_RGB,
					face->ImageWidth,
					face->ImageHeight,
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

		if (skyboxLoaded && SkyboxFacesLoading.size() > 0)
		{
			SkyboxFacesLoading.clear();
			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		}

		this->FpsCap = std::clamp(this->FpsCap, 1, 600);

		double FrameDelta = RunningTime - LastFrame;
		double FpsCapDelta = 1.f / this->FpsCap;

		// Wait the appropriate amount of time between frames
		if (!VSync && (FrameDelta < FpsCapDelta))
			SDL_Delay(static_cast<uint32_t>((FpsCapDelta - FrameDelta) * 1000));

		double DeltaTime = RunningTime - LastTime;
		LastTime = RunningTime;
		FrameStart = RunningTime;

		this->OnFrameStart.Fire(DeltaTime);

		while (SDL_PollEvent(&PollingEvent) != 0)
		{
			ImGui_ImplSDL2_ProcessEvent(&PollingEvent);

			switch (PollingEvent.type) 
			{

			case (SDL_QUIT):
			{
				this->Exit = true;
				break;
			}

			case (SDL_WINDOWEVENT):
			{
				switch (PollingEvent.window.event)
				{

				case (SDL_WINDOWEVENT_RESIZED):
				{
					int NewSizeX = PollingEvent.window.data1;
					int NewSizeY = PollingEvent.window.data2;

					// Only call ChangeResolution if the new resolution is actually different
					if (NewSizeX != this->WindowSizeX || NewSizeY != this->WindowSizeY)
						this->OnWindowResized(NewSizeX, NewSizeY);

					break;
				}

				}
			}

			}
		}

		DataModel->Update(DeltaTime);
		UpdateDescendants(dynamic_cast<GameObject*>(DataModel), DeltaTime);

		double AspectRatio = (double)this->WindowSizeX / (double)this->WindowSizeY;

		GameObject* workspaceBase = dynamic_cast<GameObject*>(Workspace);
		Object_Camera* SceneCamera = this->Workspace->GetSceneCamera();

		glm::mat4 CameraMatrix = SceneCamera->GetMatrixForAspectRatio(AspectRatio);
		float* CamMatrixPtr = glm::value_ptr(CameraMatrix);

		// Aggregate mesh and light data into a list
		Scene.MeshData = compileMeshData(workspaceBase, SceneCamera);
		Scene.LightData = compileLightData(workspaceBase);

		// Hashmap better than linaer serch
		std::unordered_map<ShaderProgram*, ShaderProgram*> uniqueShaderMap;

		for (MeshData_t md : Scene.MeshData)
		{
			if (uniqueShaderMap.find(md.Material->Shader) == uniqueShaderMap.end())
				uniqueShaderMap.insert(std::pair(md.Material->Shader, md.Material->Shader));
		}

		for (auto& it : uniqueShaderMap)
		{
			ShaderProgram* shp = it.second;

			shp->Activate();

			glUniformMatrix4fv(
				glGetUniformLocation(shp->ID, "CameraMatrix"),
				1,
				GL_FALSE,
				CamMatrixPtr
			);

			glUniform1f(glGetUniformLocation(shp->ID, "Time"), (float)RunningTime);
			Scene.UniqueShaders.push_back(shp);
		}

		glActiveTexture(GL_TEXTURE3);

		glDepthFunc(GL_LEQUAL);

		SkyboxShaders->Activate();

		glUniform1i(glGetUniformLocation(SkyboxShaders->ID, "SkyCubemap"), 3);

		glm::vec3 CamPos = glm::vec3(SceneCamera->Transform[3]);
		glm::vec3 CamForward = glm::vec3(SceneCamera->Transform[2]);

		glm::mat4 view = glm::mat4(1.f);
		glm::mat4 projection = glm::mat4(1.0f);

		// We make the mat4 into a mat3 and then a mat4 again in order to get rid of the last row and column
		// The last row and column affect the translation of the skybox (which we don't want to affect)

		view = glm::mat4(glm::mat3(glm::lookAt(CamPos, CamPos + CamForward, glm::vec3(0.f, 1.f, 0.f))));
		
		projection = glm::perspective(
			glm::radians(SceneCamera->FieldOfView),
			AspectRatio,
			SceneCamera->NearPlane,
			SceneCamera->FarPlane
		);

		glUniformMatrix4fv(
			glGetUniformLocation(SkyboxShaders->ID, "CameraMatrix"),
			1,
			GL_FALSE,
			glm::value_ptr(projection * view)
		);

		glBindTexture(GL_TEXTURE_CUBE_MAP, SkyboxCubemap);
		
		RendererContext->Framebuffer->Bind();

		glClearColor(0.086f, 0.105f, 0.21f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Skybox mesh winding order means that "BackFace" is outside the mesh.
		RendererContext->DrawMesh(&SkyboxMesh,
			SkyboxShaders,
			Vector3::one * 15.f,
			projection * view,
			FaceCullingMode::None
		);

		glDepthFunc(GL_LESS);

		//Main render pass
		RendererContext->DrawScene(Scene);

		glDisable(GL_DEPTH_TEST);

		this->OnFrameRenderGui.Fire(DeltaTime);

		glEnable(GL_DEPTH_TEST);

		//Do framebuffer stuff after everything is drawn

		RendererContext->Framebuffer->Unbind();

		this->PostProcessingShaders->Activate();

		if (EngineJsonConfig.value("postfx_enabled", false))
		{
			glUniform1i(glGetUniformLocation(PostProcessingShaders->ID, "PostFXEnabled"), 1);

			auto VignetteBlurLoc = glGetUniformLocation(PostProcessingShaders->ID, "ScreenEdgeBlurEnabled");
			auto DistortionLoc = glGetUniformLocation(PostProcessingShaders->ID, "DistortionEnabled");
			auto TimeLoc = glGetUniformLocation(PostProcessingShaders->ID, "Time");

			auto BlurVignetteStrength = glGetUniformLocation(PostProcessingShaders->ID, "BlurVignetteStrength");
			auto BlurVignetteDistMul = glGetUniformLocation(PostProcessingShaders->ID, "BlurVignetteDistMul");
			auto BlurVignetteDistExp = glGetUniformLocation(PostProcessingShaders->ID, "BlurVignetteDistExp");
			auto BlurVignetteSampleRadius = glGetUniformLocation(PostProcessingShaders->ID, "BlurVignetteSampleRadius");

			glUniform1i(VignetteBlurLoc, int(EngineJsonConfig.value("postfx_blurvignette", false)));
			glUniform1i(DistortionLoc, int(EngineJsonConfig.value("postfx_distortion", false)));

			if (EngineJsonConfig.find("postfx_blurvignette_blurstrength") != EngineJsonConfig.end())
			{
				glUniform1f(BlurVignetteStrength, (float)EngineJsonConfig["postfx_blurvignette_blurstrength"]);
				glUniform1f(BlurVignetteDistMul, (float)EngineJsonConfig["postfx_blurvignette_weightexp"]);
				glUniform1f(BlurVignetteDistExp, (float)EngineJsonConfig["postfx_blurvignette_weightmul"]);
				glUniform1i(BlurVignetteSampleRadius, (int)EngineJsonConfig["postfx_blurvignette_sampleradius"]);
			}

			glUniform1f(TimeLoc, static_cast<float>(RunningTime));
			
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, DistortionTexture->Identifier);

			glUniform1i(glGetUniformLocation(PostProcessingShaders->ID, "DistortionTexture"), 2);
		}
		else
		{
			glUniform1i(glGetUniformLocation(PostProcessingShaders->ID, "PostFXEnabled"), 0);
		}

		glUniform1i(glGetUniformLocation(PostProcessingShaders->ID, "Texture"), 1);
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

		this->m_DrawnFramesInSecond++;

		this->OnFrameEnd.Fire(DeltaTime);

		if (RunningTime - LastSecond > 1.0f)
		{
			LastSecond = RunningTime;

			this->FramesPerSecond = this->m_DrawnFramesInSecond;

			this->m_DrawnFramesInSecond = -1;

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
