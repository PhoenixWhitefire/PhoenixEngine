/*

Phoenix Engine

The oldest files I can find show that I started working on this on the 2nd of November, 2021.
Today, it is the 5th of March, 2023

This is a melting, sphaghetti code hodgepodge of random things I thought was cool when I was writing it
that SOMEHOW works without crashing atleast 50 times a frame.

Anyway, here is a small tour:

- All shader files have a file extension of one of the following: .vert, .frag
- The "main" shaders are the two worldUber.vert and worldUber.frag ones
- "phoenix.conf" contains some configuration. Set "Developer" to "false" to disable the debug UIs.
- WASD to move horizontally, Q/E to move down/up. Left-click to look around. Right-click to stop. LShift to move slower.
- Hold `R` to disable distance culling
- Press `J` to dump object hierarchy. `I` to dump GameObject API.
- F11 to toggle fullscreen.

https://github.com/Phoenixwhitefire/PhoenixEngine

*/

#define SDL_MAIN_HANDLED

#include<filesystem>
#include<imgui/backends/imgui_impl_opengl3.h>
#include<imgui/backends/imgui_impl_sdl2.h>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

#include"Engine.hpp"
#include"GlobalJsonConfig.hpp"
#include"SceneFormat.hpp"
#include"UserInput.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"
#include"editor/Editor.hpp"

static bool FirstDragFrame = false;

static bool PreviouslyPressingF11 = false;
static bool IsInputBeingSunk = false;
static bool MouseCaptured = false;

static ImGuiIO* m_imGuiIO = nullptr;

static const float MouseSensitivity = 100.0f;
static const float MovementSpeed = 15.f;

static Editor* EditorContext = nullptr;
static EngineObject* EngineInstance = nullptr;

static int PrevMouseX, PrevMouseY = 0;

static glm::tvec3<double, glm::highp> CamForward = glm::vec3(0.f, 0.f, -1.f);

static const char* ImGuiErrLn1 =
"Dear ImGui has detected a version mis-match between the compiled headers{}{}{}{}{}";
static const char* ImGuiErrLn2 =
" and the linked library. Please ensure version";
static const char* ImGuiErrLn3 =
" (#";
static const char* ImGuiErrLn4 = ") is linked.";

static void logSdlVersion()
{
	SDL_version sdlCompiledVersion{};
	SDL_version sdlDynamicVersion{};

	SDL_VERSION(&sdlCompiledVersion);
	SDL_GetVersion(&sdlDynamicVersion);

	std::string sdlCompiledVersionStr = std::vformat(
		"{}.{}.{}",
		std::make_format_args(
			sdlCompiledVersion.major,
			sdlCompiledVersion.minor,
			sdlCompiledVersion.patch
		)
	);

	std::string sdlDynamicVersionStr = std::vformat(
		"{}.{}.{}",
		std::make_format_args(
			sdlDynamicVersion.major,
			sdlDynamicVersion.minor,
			sdlDynamicVersion.patch
		)
	);

	Debug::Log(std::vformat(
		"SDL version:\n\tCompiled with: {}\n\tCurrent: {}",
		std::make_format_args(
			sdlCompiledVersionStr,
			sdlDynamicVersionStr
		)
	));
}

static int FindArgumentInCliArgs(
	int ArgCount,
	char** Arguments,
	const char* SeekingArgument
)
{
	// linear search through all cli arguments
	for (int CurrentArgIdx = 0; CurrentArgIdx < ArgCount; CurrentArgIdx++)
	{
		// strcmp makes no sense, why does it return 0 if they match and not 1??
		if (strcmp(Arguments[CurrentArgIdx], SeekingArgument) == 0)
		{
			return CurrentArgIdx;
		}
	}

	// argument not found
	return 0;
}

static void HandleInputs(Reflection::GenericValue Data)
{
	double DeltaTime = Data.Double;

	if (EngineJsonConfig.value("Developer", false))
		EditorContext->Update(DeltaTime, EngineInstance->Workspace->GetSceneCamera()->Transform);

	Object_Camera* Camera = EngineInstance->Workspace->GetSceneCamera();

	const glm::tvec3<double, glm::highp> UpVec = Vector3::yAxis;
	
	UserInput::InputBeingSunk = m_imGuiIO->WantCaptureKeyboard;

	if (!UserInput::InputBeingSunk)
	{
		double DisplacementSpeed = MovementSpeed * DeltaTime;

		if (UserInput::IsKeyDown(SDLK_LSHIFT))
			DisplacementSpeed *= 0.5f;

		Vector3 Displacement{};

		if (UserInput::IsKeyDown(SDLK_w))
			Displacement += Vector3(0, 0, DisplacementSpeed);

		if (UserInput::IsKeyDown(SDLK_a))
			Displacement += Vector3(DisplacementSpeed, 0, 0);

		if (UserInput::IsKeyDown(SDLK_s))
			Displacement += Vector3(0, 0, -DisplacementSpeed);

		if (UserInput::IsKeyDown(SDLK_d))
			Displacement += Vector3(-DisplacementSpeed, 0, 0);

		if (UserInput::IsKeyDown(SDLK_q))
			Displacement += Vector3(0, -DisplacementSpeed, 0);

		if (UserInput::IsKeyDown(SDLK_e))
			Displacement += Vector3(0, DisplacementSpeed, 0);

		Camera->Transform = glm::translate(Camera->Transform, glm::vec3((glm::tvec3<double>)Displacement));
	}

	int mouseX;
	int mouseY;

	SDL_Window* window = EngineInstance->Window;

	uint32_t activeMouseButton = SDL_GetMouseState(&mouseX, &mouseY);

	IsInputBeingSunk = m_imGuiIO->WantCaptureKeyboard || m_imGuiIO->WantCaptureMouse;

	if (MouseCaptured)
	{
		// Doesn't hide unless we tell Dear ImGui to hide the cursor too
		// (Otherwise it flickers)
		// 22/08/2024
		ImGui::SetMouseCursor(ImGuiMouseCursor_None);
		SDL_ShowCursor(SDL_DISABLE);

		int windowSizeX, windowSizeY;

		SDL_GetWindowSize(window, &windowSizeX, &windowSizeY);

		if (FirstDragFrame)
		{
			
			SDL_SetWindowMouseGrab(window, SDL_TRUE);

			SDL_WarpMouseInWindow(window, windowSizeX / 2, windowSizeY / 2);

			SDL_GetMouseState(&mouseX, &mouseY);

			PrevMouseX = windowSizeX / 2;
			PrevMouseY = windowSizeY / 2;

			FirstDragFrame = false;
		}

		int deltaMouseX = PrevMouseX - mouseX;
		int deltaMouseY = PrevMouseY - mouseY;

		double RotationX = MouseSensitivity * ((double)deltaMouseY - (windowSizeY / 2.0f)) / windowSizeY;
		double RotationY = MouseSensitivity * ((double)deltaMouseX - (windowSizeX / 2.0f)) / windowSizeX;
		RotationX += 50.f; // TODO 22/08/2024: Why??
		RotationY += 50.f;

		glm::tvec3<double, glm::highp> newForward = glm::rotate(
			CamForward,
			glm::radians(RotationX),
			glm::normalize(glm::cross(CamForward, UpVec))
		);

		if (abs(glm::angle(newForward, UpVec) - glm::radians(90.0f)) <= glm::radians(85.0f))
			CamForward = newForward;

		CamForward = glm::rotate(CamForward, glm::radians(-RotationY), UpVec);

		glm::tvec3<double, glm::highp> Position{ Camera->Transform[3] };

		Camera->Transform = glm::lookAt(Position, Position + CamForward, UpVec);

		// Keep the mouse in the window.
		// Teleport it to the other side if it hits the edge.

		int newMouseX = mouseX, newMouseY = mouseY;

		if (mouseX <= 10)
			newMouseX = windowSizeX - 20;

		if (mouseX >= windowSizeX - 10)
			newMouseX = 20;

		if (mouseY <= 10)
			newMouseY = windowSizeY - 20;

		if (mouseY >= windowSizeY - 10)
			newMouseY = 20;

		if (IsInputBeingSunk)
			newMouseX, newMouseY = windowSizeX / 2, windowSizeY / 2;

		if (newMouseX != mouseX || newMouseY != mouseY)
		{
			SDL_WarpMouseInWindow(window, newMouseX, newMouseY);
			mouseX = newMouseX;
			mouseY = newMouseY;
		}

		if (activeMouseButton & SDL_BUTTON_RMASK)
		{
			MouseCaptured = false;
			SDL_WarpMouseInWindow(window, windowSizeX / 2, windowSizeY / 2);
		}
	}
	else
	{
		if (!FirstDragFrame)
		{
			SDL_ShowCursor(SDL_ENABLE);
			SDL_SetWindowMouseGrab(window, SDL_FALSE);

			ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

			FirstDragFrame = true;
		}

		if (activeMouseButton & SDL_BUTTON_LMASK && !IsInputBeingSunk)
			MouseCaptured = true;
	}

	PrevMouseX = mouseX;
	PrevMouseY = mouseY;

	if (UserInput::IsKeyDown(SDLK_F11))
		if (!PreviouslyPressingF11)
		{
			PreviouslyPressingF11 = true;
			EngineInstance->SetIsFullscreen(!EngineInstance->IsFullscreen);
		}
	else
		PreviouslyPressingF11 = false;
}

static char* LevelLoadPathBuf;
static char* LevelSavePathBuf;

static void LoadLevel(const std::string& LevelPath)
{
	Debug::Log(std::vformat("Loading scene: '{}'", std::make_format_args(LevelPath)));

	GameObject* workspace = GameObject::s_DataModel->GetChild("Workspace");

	GameObject* prevModel = workspace->GetChild("Level");

	if (prevModel)
		prevModel->Destroy();

	GameObject* levelModel = GameObject::CreateGameObject("Model");
	levelModel->Name = "Level";
	levelModel->SetParent(workspace);

	bool loadSuccess = true;

	std::vector<GameObject*> Objects = SceneFormat::Deserialize(FileRW::ReadFile(LevelPath), &loadSuccess);

	if (loadSuccess)
	{
		for (GameObject* object : Objects)
			object->SetParent(levelModel);
	}
	else
		throw("Failed to load level: " + SceneFormat::GetLastErrorString());

	//MapLoader::LoadMapIntoObject(LevelPath, levelModel);
}

static void DrawUI(Reflection::GenericValue Data)
{
	if (UserInput::IsKeyDown(SDLK_j) && !IsInputBeingSunk)
	{
		printf("BEGIN DUMP WORLD ARRAY\n");
		printf("%zi OBJECTS: \n", GameObject::s_WorldArray.size());

		for (auto& pair : GameObject::s_WorldArray)
			printf(
				"%i: %s '%s', Parent %i\n",
				pair.first,
				pair.second->ClassName.c_str(),
				pair.second->Name.c_str(),
				pair.second->Parent
			);

		printf("END DUMP WORLD ARRAY\n");
	}

	if (UserInput::IsKeyDown(SDLK_l) && !IsInputBeingSunk)
	{
		Debug::Log("Dumping GameObject API...");

		auto dump = GameObject::DumpApiToJson();
		FileRW::WriteFile("apidump.json", dump.dump(2), false);

		Debug::Log("API dump finished");
	}

	if (EditorContext)
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Level");

		static std::string saveMessage;
		static double saveMessageTimeLeft;

		ImGui::InputText("Save target", LevelSavePathBuf, 64);

		if (ImGui::Button("Save"))
		{
			GameObject* levelModel = EngineInstance->Workspace->GetChild("Level");

			if (levelModel)
			{
				std::string levelSavePath(LevelSavePathBuf);

				bool alreadyExists = false;
				std::string prevFile = FileRW::ReadFile(levelSavePath, &alreadyExists);

				if (alreadyExists && prevFile.find("#Version 02.00") == std::string::npos)
				{
					// V2 does not have full parity with V1 as of 02/09/2024
					// (models)
					saveMessage = "Not saving as that file already exists as a V1";
					saveMessageTimeLeft = 1.5;
				}
				else
				{
					std::string serialized = SceneFormat::Serialize(levelModel->GetChildren(), levelSavePath);
					FileRW::WriteFile(levelSavePath, serialized, true);

					saveMessage = std::vformat("Saved as '{}'", std::make_format_args(levelSavePath));
					saveMessageTimeLeft = 1.f;
				}
			}
			else
			{
				saveMessage = "Save failed as Workspace had no `Level` child";
				saveMessageTimeLeft = 1.f;
			}
		}

		if (saveMessageTimeLeft > 0.f)
		{
			ImGui::Text(saveMessage.c_str());
			saveMessageTimeLeft -= Data.AsDouble();
		}

		ImGui::InputText("Load target", LevelLoadPathBuf, 64);

		if (ImGui::Button("Load"))
			LoadLevel(LevelLoadPathBuf);

		ImGui::End();

		ImGui::Begin("Info");

		ImGui::Text("FPS: %d", EngineInstance->FramesPerSecond);
		ImGui::Text("Frame time: %dms", (int)ceil(EngineInstance->FrameTime * 1000));

		ImGui::End();

		ImGui::Begin("Settings");

		ImGui::Checkbox("VSync", &EngineInstance->VSync);

		bool WasFullscreen = EngineInstance->IsFullscreen;

		ImGui::Checkbox("Fullscreen", &EngineInstance->IsFullscreen);

		if (EngineInstance->IsFullscreen != WasFullscreen)
			EngineInstance->SetIsFullscreen(EngineInstance->IsFullscreen);

		if (EngineInstance->VSync)
			SDL_GL_SetSwapInterval(1);
		else
		{
			SDL_GL_SetSwapInterval(0);

			ImGui::InputInt("FPS max", &EngineInstance->FpsCap, 1, 30);
		}

		bool PostFXEnabled = EngineJsonConfig.value("postfx_enabled", false);

		ImGui::Checkbox("Post-Processing", &PostFXEnabled);

		EngineJsonConfig["postfx_enabled"] = PostFXEnabled;

		if (PostFXEnabled)
		{
			bool BlurVignette = EngineJsonConfig.value("postfx_blurvignette", false);
			bool Distortion = EngineJsonConfig.value("postfx_distortion", false);

			ImGui::Checkbox("Blur vignette", &BlurVignette);
			ImGui::Checkbox("Distortion", &Distortion);

			EngineJsonConfig["postfx_blurvignette"] = BlurVignette;
			EngineJsonConfig["postfx_distortion"] = Distortion;

			if (EngineJsonConfig["postfx_blurvignette"])
			{
				float DistFactorMultiplier = EngineJsonConfig.value("postfx_blurvignette_blurstrength", 2.f);
				float WeightExponent = EngineJsonConfig.value("postfx_blurvignette_weightexp", 16.f);
				float WeightMultiplier = EngineJsonConfig.value("postfx_blurvignette_weightmul", 2.5f);
				float SampleRadius = EngineJsonConfig.value("postfx_blurvignette_sampleradius", 4.f);

				ImGui::InputFloat("Vignette dist weight factor", &DistFactorMultiplier);
				ImGui::InputFloat("Vignette weight exponent", &WeightExponent);
				ImGui::InputFloat("Vignette weight multiplier", &WeightMultiplier);
				ImGui::InputFloat("Vignette sample radius", &SampleRadius);

				EngineJsonConfig["postfx_blurvignette_blurstrength"] = DistFactorMultiplier;
				EngineJsonConfig["postfx_blurvignette_weightexp"] = WeightExponent;
				EngineJsonConfig["postfx_blurvignette_weightmul"] = WeightMultiplier;
				EngineJsonConfig["postfx_blurvignette_sampleradius"] = SampleRadius;
			}
		}

		bool SaveConfig = ImGui::Button("Save Post FX settings");

		if (SaveConfig)
		{
			FileRW::WriteFile("phoenix.conf", EngineJsonConfig.dump(2), false);
			Debug::Log("The JSON Config overwrote the pre-existing 'phoenix.conf'.");
		}

		ImGui::End();

		EditorContext->RenderUI();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	IsInputBeingSunk = m_imGuiIO->WantCaptureKeyboard || m_imGuiIO->WantCaptureMouse;
}

static void Application(int argc, char** argv)
{
	const char* imGuiVersion = IMGUI_VERSION;
	int imGuiVersionNum = IMGUI_VERSION_NUM;

	Debug::Log(std::vformat(
		"Initializing our Dearest ImGui {} (#{})...",
		std::make_format_args(imGuiVersion, imGuiVersionNum)
	));

	bool imGuiVersionCorrect = IMGUI_CHECKVERSION();

	if (!imGuiVersionCorrect)
	{
		throw(std::vformat(
			ImGuiErrLn1,
			std::make_format_args(
				ImGuiErrLn2,
				imGuiVersion,
				ImGuiErrLn3,
				imGuiVersionNum,
				ImGuiErrLn4
			)
		));
	}

	ImGui::CreateContext();

	m_imGuiIO = &ImGui::GetIO();
	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(EngineInstance->Window, EngineInstance->RendererContext->GLContext);
	ImGui_ImplOpenGL3_Init("#version 460");

	EditorContext = EngineJsonConfig.value("Developer", false) ? new Editor() : nullptr;
	
	LevelLoadPathBuf = (char*)malloc(64);
	LevelSavePathBuf = (char*)malloc(64);

	if (!LevelLoadPathBuf)
		throw("Could not allocate buffer for Level load path");

	if (!LevelSavePathBuf)
		throw("Could not allocate buffer for Level save path");

	static const char* defaultLoadLevel = "levels/dev.world";
	static const char* defaultSaveLevel = "levels/save.world";

	for (int i = 0; i < 64; i++)
		LevelLoadPathBuf[i] = i < strlen(defaultLoadLevel)
								? defaultLoadLevel[i]
								: '\0';

	for (int i = 0; i < 64; i++)
		LevelSavePathBuf[i] = i < strlen(defaultSaveLevel)
								? defaultSaveLevel[i]
								: '\0';

	const char* mapFileFromArgs{};
	bool hasMapFromArgs = false;

	int mapFileArgIdx = FindArgumentInCliArgs(argc, argv, "-loadmap");

	if (mapFileArgIdx > 0)
	{
		Debug::Log(std::vformat(
			"Map to load specified from launch argument. Map was: {}",
			std::make_format_args(argv[mapFileArgIdx + 1])
		));

		mapFileFromArgs = argv[mapFileArgIdx + 1];
		hasMapFromArgs = true;
	}

	std::string mapFile = hasMapFromArgs ?
							mapFileFromArgs
							: EngineJsonConfig.value("RootScene", "levels/empty.world");

	LoadLevel(mapFile);

	if (EditorContext)
		EditorContext->Init();

	EngineInstance->OnFrameStart.Connect(HandleInputs);
	EngineInstance->OnFrameRenderGui.Connect(DrawUI);

	EngineInstance->Start();

	// After the Main Loop exits

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();

	// Engine destructor is called as the `EngineObject`'s scope terminates in `main`.
}

static void handleCrash(std::string Error, std::string ExceptionType)
{
	auto fmtArgs = std::make_format_args(
		ExceptionType,
		Error,
		"If this is the first time this has happened, please re-try. Otherwise, contact the developers."
	);

	Debug::Log(std::vformat("CRASH - {}: {}", fmtArgs));

	std::string errMessage = std::vformat(
		"An unexpected error occurred, and the application will now close. Details:\n\nType: {}\nError: {}\n\n{}",
		fmtArgs
	);

	SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_ERROR,
		"Engine error",
		errMessage.c_str(),
		nullptr
	);
}

int main(int argc, char** argv)
{
	Debug::Log("Application initializing...");

	logSdlVersion();

	SDL_Window* window = nullptr;

	try
	{
		EngineObject engine{};
		EngineInstance = &engine;
		window = engine.Window;

		//Engine->MSAASamples = 2;

		Application(argc, argv);

		Debug::Save(); // in case FileRW::WriteFile throws an exception
	}
	catch (std::string Error)
	{
		handleCrash(Error, "std::string");
	}
	catch (const char* Error)
	{
		handleCrash(Error, "const char*");
	}
	catch (std::filesystem::filesystem_error Error)
	{
		handleCrash(Error.what(), "std::filesystem::filesystem_error");
	}

	Debug::Log("Application closing...");

	Debug::Save();
}