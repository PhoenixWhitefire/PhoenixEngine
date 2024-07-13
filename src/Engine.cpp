#include<chrono>
#include<string>
#include<format>
#include<glad/gl.h>
#include<stb_image.h>
#include<imgui/imgui_impl_sdl.h>
#include<glm/matrix.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtc/matrix_transform.hpp>

#include"Engine.hpp"
#include"ThreadManager.hpp"
#include"Debug.hpp"
#include"FileRW.hpp"

EngineObject* EngineObject::Singleton = nullptr;

uint32_t RectangleVAO, RectangleVBO;
static auto ChronoStartTime = std::chrono::high_resolution_clock::now();

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

static void UpdateDescendants(std::shared_ptr<GameObject> root, double DeltaTime)
{
	for (auto& obj : root->GetChildren())
	{
		if (!obj->DidInit)
			obj->Initialize();
		obj->DidInit = true;

		obj->Update(DeltaTime);

		if (obj->GetChildren().size() > 0)
			UpdateDescendants(obj, DeltaTime);
	}
}

static void StepPhysicsForObjects(std::vector<std::shared_ptr<GameObject>> Objects, PhysicsSolver& Physics, double Delta)
{
	for (uint32_t Index = 0; Index < Objects.size(); Index++)
	{
		std::shared_ptr<Object_Base3D> Object = std::dynamic_pointer_cast<Object_Base3D>(Objects[Index]);

		if (Object)
		{
			Physics.ComputePhysicsForObject(Object, Delta);
		}
	}
}

void EngineObject::ResizeWindow(int NewSizeX, int NewSizeY)
{
	this->WindowSizeX = NewSizeX;
	this->WindowSizeY = NewSizeY;

	SDL_SetWindowSize(this->Window, NewSizeX, NewSizeY);

	SDL_SetWindowPosition(this->Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

	this->m_renderer->ChangeResolution(NewSizeX, NewSizeY);
}

int WindowSizeXBeforeFullscreen = 800;
int WindowSizeYBeforeFullscreen = 800;
int WindowPosXBeforeFullscreen = SDL_WINDOWPOS_CENTERED;
int WindowPosYBeforeFullscreen = SDL_WINDOWPOS_CENTERED;

void EngineObject::SetIsFullscreen(bool Fullscreen)
{
	this->IsFullscreen = Fullscreen;

	if (Fullscreen)
	{
		WindowSizeXBeforeFullscreen = this->WindowSizeX;
		WindowSizeYBeforeFullscreen = this->WindowSizeY;

		//SDL_GetWindowSize(this->Window, &WindowSizeXBeforeFullscreen, &WindowSizeYBeforeFullscreen);
		SDL_GetWindowPosition(this->Window, &WindowPosXBeforeFullscreen, &WindowPosYBeforeFullscreen);

		SDL_DisplayMode Mode;

		SDL_GetCurrentDisplayMode(0, &Mode);

		SDL_SetWindowSize(this->Window, Mode.w, Mode.h);

		this->WindowSizeX = Mode.w;
		this->WindowSizeY = Mode.h;

		this->ResizeWindow(Mode.w, Mode.h);
	}
	else
	{
		SDL_SetWindowSize(this->Window, WindowSizeXBeforeFullscreen, WindowSizeYBeforeFullscreen);
		SDL_SetWindowPosition(this->Window, WindowPosXBeforeFullscreen, WindowPosYBeforeFullscreen);

		this->WindowSizeX = WindowSizeXBeforeFullscreen;
		this->WindowSizeY = WindowSizeYBeforeFullscreen;

		this->ResizeWindow(WindowPosXBeforeFullscreen, WindowPosYBeforeFullscreen);
	}

	SDL_SetWindowFullscreen(this->Window, this->IsFullscreen ? SDL_WINDOW_FULLSCREEN : this->SDLWindowFlags);
}

EngineObject::EngineObject(Vector2 WindowStartSize, SDL_Window** WindowPtr)
{
	this->m_DrawCallBatch = nullptr;
	EngineObject::Singleton = this;

	//GameObject::GameObjectTable["Model"] = &Object_Model()

	this->WindowSizeX = WindowStartSize.X;
	this->WindowSizeY = WindowStartSize.Y;

	SDL_Init(SDL_INIT_VIDEO);
	
	//Initialize SDL
	this->Window = SDL_CreateWindow(
		"Waiting on configuration...",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		(int)WindowStartSize.X, (int)WindowStartSize.Y,
		this->SDLWindowFlags
	);

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
		throw(std::string("Could not find configuration file (phoenix.conf)!"));

	if (ConfigAscii == "")
		throw(std::string("Configuration file phoenix.conf is not configured (empty)"));

	this->GameConfig = EngineJsonConfig;

	SDL_SetWindowSize(this->Window, this->GameConfig["DefaultWindowSize"][0], this->GameConfig["DefaultWindowSize"][1]);

	SDL_SetWindowTitle(this->Window, std::string(this->GameConfig["WindowTitle"]).c_str());

	if (WindowPtr)
		*WindowPtr = this->Window;

	if (!this->Window)
	{
		const char* errStr = SDL_GetError();
		throw(std::vformat("SDL error: Could not create window: {}\n", std::make_format_args(errStr)));
	}

	// TODO: Engine->MSAASamples does nothing, attempting to specify via below ctor's argument leads to
	// OpenGL error "Target doesn't match the texture's target"
	this->m_renderer = new Renderer(this->WindowSizeX, this->WindowSizeY, this->Window, 0);

	auto DataModel = GameObjectFactory::CreateGameObject("Workspace");

	this->Game = std::dynamic_pointer_cast<Object_Workspace>(DataModel);
	GameObject::DataModel = DataModel;

	this->PostProcessingShaders = ShaderProgram::GetShaderProgram("postprocessing");

	this->PostProcessingShaders->Activate();
	glUniform1i(glGetUniformLocation(this->PostProcessingShaders->ID, "Texture"), 0);

	glEnable(GL_DEPTH_TEST);

	//Post-processing framebuffer

	glGenVertexArrays(1, &RectangleVAO);
	glGenBuffers(1, &RectangleVBO);
	glBindVertexArray(RectangleVAO);
	glBindBuffer(GL_ARRAY_BUFFER, RectangleVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(RectangleVertices), &RectangleVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	ThreadManager::Get()->ApplicationWindow = this->Window;

	ThreadManager::Get()->CreateWorkers(4, WorkerType::DefaultTaskWorker);
}

EngineObject* EngineObject::Get(Vector2 WindowStartSize, SDL_Window** WindowPtr)
{
	if (EngineObject::Singleton != nullptr)
		return EngineObject::Singleton;
	else
		return new EngineObject(WindowStartSize, WindowPtr);
}

EngineObject* EngineObject::Get()
{
	if (EngineObject::Singleton != nullptr)
		return EngineObject::Singleton;
	else
		return new EngineObject(Vector2(800, 800), nullptr);
}

std::shared_ptr<Object_Base3D> LastDrawn;

std::vector<LightData_t> EngineObject::m_compileLightData(std::shared_ptr<GameObject> RootObject)
{
	std::vector<std::shared_ptr<GameObject>> Objects = RootObject->GetChildren();

	std::vector<LightData_t> DataList;

	for (std::shared_ptr<GameObject> Object : Objects)
	{
		if (!Object->Enabled)
			continue;

		std::shared_ptr<Object_Light> LightObject = std::dynamic_pointer_cast<Object_Light>(Object);

		if (LightObject)
		{
			std::shared_ptr<Object_DirectionalLight> Directional = std::dynamic_pointer_cast<Object_DirectionalLight>(LightObject);
			std::shared_ptr<Object_PointLight> Point = std::dynamic_pointer_cast<Object_PointLight>(LightObject);

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
			std::vector<LightData_t> ChildData = this->m_compileLightData(Object);

			for (uint32_t CDataIdx = 0; CDataIdx < ChildData.size(); CDataIdx++)
				DataList.push_back(ChildData[CDataIdx]);
		}
	}

	return DataList;
}

std::vector<MeshData_t> EngineObject::m_compileMeshData(std::shared_ptr<GameObject> RootObject)
{
	std::vector<std::shared_ptr<GameObject>> Objects = RootObject->GetChildren();

	std::vector<MeshData_t> DataList;

	std::shared_ptr<Object_Camera> SceneCamera = this->Game->GetSceneCamera();

	for (std::shared_ptr<GameObject> Object : Objects)
	{
		if (Object->Enabled)
		{
			std::shared_ptr<Object_Base3D> Object3D = std::dynamic_pointer_cast<Object_Base3D>(Object);

			if (Object3D != nullptr)
			{
				// TODO: frustum culling
				// Hold R to disable distance culling
				if ((Vector3(glm::vec3(Object3D->Matrix[3])) - glm::vec3(SceneCamera->Matrix[3])).Magnitude > 100.0f
					&& !UserInput::IsKeyDown(SDLK_r))
					continue;

				//TODO: recheck whether we need this
				// if (MeshObject->HasTransparency)
					//continue;

				glm::mat4 ModelMatrix = glm::mat4(1.0f);

				std::shared_ptr<Object_Model> ParentModel = std::dynamic_pointer_cast<Object_Model>(Object->Parent);

				//if (ParentModel)
				//	ModelMatrix = ParentModel->Matrix;

				MeshData_t Data;
				Data.MeshData = Object3D->GetRenderMesh();
				Data.Material = Object3D->Material;
				Data.Matrix = ModelMatrix * Object3D->Matrix;
				Data.Size = Object3D->Size;
				Data.Transparency = Object3D->Transparency;
				Data.Reflectivity = Object3D->Reflectivity;
				Data.TintColor = Object3D->ColorRGB;
				Data.FaceCulling = Object3D->FaceCulling;

				DataList.push_back(Data);
			}

			std::shared_ptr<Object_ParticleEmitter> PEmitterObject = std::dynamic_pointer_cast<Object_ParticleEmitter>(Object);

			if (PEmitterObject != nullptr)
				this->m_particleEmitters.push_back(PEmitterObject);

			if (Object->GetChildren().size() > 0)
			{
				std::vector<MeshData_t> ChildData = this->m_compileMeshData(Object);

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
	this->Physics->WorldGravity = Vector3::DOWN * 9.8f; // Earth's gravity is 9.8N

	// TODO:
	// wtf are these
	// 13/07/2024
	double LastTime = this->RunningTime;
	float LastFrame = GetRunningTime();
	double FrameStart = 0.0f;
	float LastSecond = LastFrame;

	this->Exit = false;

	static const std::string SkyboxCubemapImages[6] =
	{
		"Sky1/right.jpg",
		"Sky1/left.jpg",
		"Sky1/top.jpg",
		"Sky1/bottom.jpg",
		"Sky1/front.jpg",
		"Sky1/back.jpg"
	};

	static const std::string BaseTexturePath = "./resources/textures/";

	GLuint SkyboxCubemap = 0;

	glGenTextures(1, &SkyboxCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, SkyboxCubemap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	//glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_LOD_BIAS, 15.f);

	for (int ImageIndex = 0; ImageIndex < 6; ImageIndex++)
	{
		int Width, Height, NumberChannels;

		std::string ImagePath = BaseTexturePath + SkyboxCubemapImages[ImageIndex];

		// TODO: replace with TextureManager calls
		// 13/07/2024
		uint8_t* ImageBytes = stbi_load(
			ImagePath.c_str(),
			&Width,
			&Height,
			&NumberChannels,
			0
		);

		if (ImageBytes != nullptr)
		{
			stbi_set_flip_vertically_on_load(false);

			glTexImage2D(
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + ImageIndex,
				0,
				GL_RGB,
				Width,
				Height,
				0,
				GL_RGB,
				GL_UNSIGNED_BYTE,
				ImageBytes
			);

			glGenerateMipmap(GL_TEXTURE_2D);
			//glGenerateMipmap(GL_TEXTURE_CUBE_MAP_POSITIVE_X + ImageIndex);

			stbi_image_free(ImageBytes);
		}
		else
			Debug::Log("Failed to load image for skybox cubemap: '" + ImagePath + "'");
	}

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	//Texture HDRISkyboxTexture("Assets/Textures/hdri_sky_1.jpeg", "Diffuse", 0);

	ShaderProgram SkyboxShaders = *ShaderProgram::GetShaderProgram("skybox");

	SkyboxShaders.Activate();

	glUniform1i(glGetUniformLocation(SkyboxShaders.ID, "SkyCubemap"), 0);

	Scene_t Scene = Scene_t();

	this->m_renderer->m_framebuffer->Unbind();
	this->m_renderer->m_framebuffer->UnbindTexture();

	Texture DistortionTexture;
	DistortionTexture.ImagePath = "resources/textures/MISSING2_MaximumADHD_status_1665776378145304579.png";
	TextureManager::Get()->CreateTexture2D(&DistortionTexture, false);

	glActiveTexture(GL_TEXTURE17);
	glBindTexture(GL_TEXTURE_2D, DistortionTexture.Identifier);

	this->PostProcessingShaders->Activate();
	glUniform1i(glGetUniformLocation(this->PostProcessingShaders->ID, "DistortionTexture"), 17);

	Mesh SkyboxMesh = Mesh(SkyboxVertices, SkyboxIndices);

	SDL_Event PollingEvent;

	Debug::Log("Main program loop start...");

	while (!this->Exit)
	{
		this->RunningTime = GetRunningTime();

		Scene.MeshData.clear();
		Scene.LightData.clear();

		// TODO texture streaming should use low-quality versions so they don't appear black!
		TextureManager::Get()->FinalizeAsyncLoadedTextures();

		this->FpsCap = std::clamp(this->FpsCap, 1, 600);

		double FrameDelta = RunningTime - LastFrame;
		double FpsCapDelta = 1.f / this->FpsCap;

		// Wait the appropriate amount of time between frames
		if (!VSync && (FrameDelta < FpsCapDelta))
			SDL_Delay((FpsCapDelta - FrameDelta) * 1000);

		double DeltaTime = RunningTime - LastTime;
		LastTime = RunningTime;
		FrameStart = RunningTime;

		this->OnFrameStart.Fire(std::make_tuple(this, DeltaTime, RunningTime));

		this->m_particleEmitters.clear();

		while (SDL_PollEvent(&PollingEvent) != 0)
		{
			ImGui_ImplSDL2_ProcessEvent(&PollingEvent);

			if (PollingEvent.type == SDL_WINDOWEVENT)
			{
				switch (PollingEvent.window.event) {

				case SDL_WINDOWEVENT_RESIZED: {
					int NewSizeX, NewSizeY;

					SDL_GetWindowSize(this->Window, &NewSizeX, &NewSizeY);

					// Only call ChangeResolution if the new resolution is actually different
					if (NewSizeX != this->WindowSizeX || NewSizeY != this->WindowSizeY)
					{
						this->WindowSizeX = NewSizeX;
						this->WindowSizeY = NewSizeY;
						this->m_renderer->ChangeResolution(PollingEvent.window.data1, PollingEvent.window.data2);
					}

					break;
				}

				}
			}

			switch (PollingEvent.type) {

			case SDL_QUIT: {
				this->Exit = true;
				break;
			}

			}
		}

		this->m_LightIndex = 0;

		glClearColor(0.086f, 0.105f, 0.21f, 1.0f);
		glClear(/*GL_COLOR_BUFFER_BIT | */ GL_DEPTH_BUFFER_BIT);

		this->Game->Update(DeltaTime);
		UpdateDescendants(Game, DeltaTime);

		StepPhysicsForObjects(this->Game->GetChildren(), *this->Physics, DeltaTime);

		double AspectRatio = (float)this->WindowSizeX / (float)this->WindowSizeY;

		std::shared_ptr<Object_Camera> SceneCamera = this->Game->GetSceneCamera();

		glm::mat4 CameraMatrix = SceneCamera->GetMatrixForAspectRatio(AspectRatio);
		float* CamMatrixPtr = glm::value_ptr(CameraMatrix);

		// Aggregate mesh and light data into a list
		Scene.MeshData = this->m_compileMeshData(this->Game);
		Scene.LightData = this->m_compileLightData(this->Game);

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

		glActiveTexture(GL_TEXTURE0);

		glDepthFunc(GL_LEQUAL);

		SkyboxShaders.Activate();

		glUniform1i(glGetUniformLocation(SkyboxShaders.ID, "SkyboxCubemap"), SkyboxCubemap);

		glm::vec3 CamPos = glm::vec3(SceneCamera->Matrix[3]);
		glm::vec3 CamForward = glm::vec3(SceneCamera->Matrix[2]);

		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 projection = glm::mat4(1.0f);
		// We make the mat4 into a mat3 and then a mat4 again in order to get rid of the last row and column
		// The last row and column affect the translation of the skybox (which we don't want to affect)
		view = glm::mat4(
			glm::mat3(
				glm::lookAt(
					CamPos,
					CamPos + CamForward,
					glm::vec3(Vector3::UP)
				)
			)
		);
		projection = glm::perspective(
			glm::radians(SceneCamera->FieldOfView),
			(float)AspectRatio,
			SceneCamera->NearPlane,
			SceneCamera->FarPlane
		);

		glUniformMatrix4fv(glGetUniformLocation(SkyboxShaders.ID, "CameraMatrix"), 1, GL_FALSE, glm::value_ptr(projection * view));

		glBindTexture(GL_TEXTURE_CUBE_MAP, SkyboxCubemap);
		
		if (EngineJsonConfig.value("postfx_enabled", false))
			m_renderer->m_framebuffer->Bind();

		// Skybox mesh winding order means that "BackFace" is outside the mesh.
		this->m_renderer->DrawMesh(&SkyboxMesh, &SkyboxShaders, Vector3::ONE, projection * view, FaceCullingMode::BackFace);

		glDepthFunc(GL_LESS);

		//Main render pass
		this->m_renderer->DrawScene(Scene);

		//Particle emitters' particles need to be drawn after scene due to transparency
		for (auto& emitter : this->m_particleEmitters)
		{
			emitter->Update(this->FrameTime);
			emitter->Render(CameraMatrix);
		}
		
		glDisable(GL_DEPTH_TEST);

		this->OnFrameRenderGui.Fire(std::make_tuple(this, DeltaTime, RunningTime));

		glEnable(GL_DEPTH_TEST);

		//Do framebuffer stuff after everything is drawn

		this->m_renderer->m_framebuffer->Unbind();

		if (EngineJsonConfig.value("postfx_enabled", false))
		{
			this->PostProcessingShaders->Activate();

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
				glUniform1i(BlurVignetteSampleRadius, (float)EngineJsonConfig["postfx_blurvignette_sampleradius"]);
			}

			glUniform1f(TimeLoc, RunningTime);
			glUniform1i(glGetUniformLocation(PostProcessingShaders->ID, "Texture"), 1);
			glUniform1i(glGetUniformLocation(PostProcessingShaders->ID, "DistortionTexture"), 2);

			glBindVertexArray(RectangleVAO);
			glDisable(GL_DEPTH_TEST);

			glActiveTexture(GL_TEXTURE0);
			m_renderer->m_framebuffer->BindTexture();

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, DistortionTexture.Identifier);
			
			glDrawArrays(GL_TRIANGLES, 0, 6);

			glEnable(GL_DEPTH_TEST);
		}

		// End of frame

		this->RunningTime = GetRunningTime();

		double curTimePrevSwap = RunningTime;

		SDL_GL_SwapWindow(this->Window);

		this->RunningTime = GetRunningTime();

		// Should be calculated before swapping buffers due to VSync
		this->FrameTime = curTimePrevSwap - FrameStart;
		LastFrame = RunningTime;

		this->m_DrawnFramesInSecond++;

		this->OnFrameEnd.Fire(std::make_tuple(this, this->FrameTime, GetRunningTime()));

		if (RunningTime - LastSecond > 1.0f)
		{
			LastSecond = RunningTime;

			this->FramesPerSecond = this->m_DrawnFramesInSecond;

			this->m_DrawnFramesInSecond = -1;
		}
	}
}

std::vector<std::shared_ptr<GameObject>>& EngineObject::LoadModelAsMeshesAsync(
	const char* ModelFilePath,
	Vector3 Size,
	Vector3 Position,
	bool AutoParent
)
{
	// TODO: fix memory leak, we need  Loader.LoadedObjects but without 'new' it gets deleted along with ModelLoader
	ModelLoader* Loader = new ModelLoader(ModelFilePath, AutoParent ? this->Game : nullptr, this->Window);

	for (int Index = 0; Index < Loader->LoadedObjects.size(); Index++)
	{
		std::shared_ptr<Object_Mesh> Object = std::dynamic_pointer_cast<Object_Mesh>(Loader->LoadedObjects[Index]);

		Object->Matrix = glm::translate(Object->Matrix, (glm::vec3)Position);

		Object->Size = Object->Size * Size;
	}

	return Loader->LoadedObjects;
}

EngineObject::~EngineObject()
{
	this->Exit = true;

	ShaderProgram::ClearAll();

	delete this->Physics;
	delete this->m_renderer;

	Debug::Log("Engine closing...");

	Debug::Save();

	// TODO fix crash
	//this->Game->~GameObject();
}
