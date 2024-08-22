/*

Phoenix Engine

The oldest files I can find show that I started working on this on the 2nd of November, 2021.
Today, it is the 5th of March, 2023

This is a melting, sphaghetti code hodgepodge of random things I thought was cool when I was writing it
that SOMEHOW works without crashing atleast 50 times a frame.

Anyway, here is a small tour:

- All shader files have a file extension of one of the following: .vert, .frag
- The "main" shaders are the two worldUber.vert and worldUber.frag ones
- "phoenix.conf" contains some configuration. Set "developer" to "false" to disable the debug UIs.
- Hold R to disable distance culling
- WASD to move horizontally, Q/E to move down/up. Right-click and drag to look. LShift to move slower.
- F11 to toggle fullscreen.


*/

#define SDL_MAIN_HANDLED

#include<iostream>
#include<cmath>
#include<math.h>
#include<nljson.h>
#include<imgui/imgui.h>
#include<imgui/imgui_impl_opengl3.h>
#include<imgui/imgui_impl_sdl.h>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

#include"editor/editor.hpp"
#include"Engine.hpp"
#include"UserInput.hpp"
#include"GlobalJsonConfig.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

static bool FirstDragFrame = false;

static bool PreviouslyPressingF11 = false;
static bool IsInputBeingSunk = false;
static bool MouseCaptured = false;

static ImGuiIO* m_imGuiIO = nullptr;

static const float MouseSensitivity = 100.0f;
static const float MovementSpeed = 15.f;

static Editor* EditorContext = nullptr;

static int PrevMouseX, PrevMouseY = 0;

static glm::vec3 CamForward = glm::vec3(0.f, 0.f, -1.f);

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

static void HandleInputs(std::tuple<EngineObject*, double, double> Data)
{

	EngineObject* Engine = std::get<0>(Data);
	double DeltaTime = std::get<1>(Data);
	double CurrentTime = std::get<2>(Data);

	if (EngineJsonConfig.value("developer", false))
		EditorContext->Update(DeltaTime, Engine->Workspace->GetSceneCamera()->Matrix);

	Object_Camera* Camera = Engine->Workspace->GetSceneCamera();

	const glm::vec3 UpVec = Vector3::yAxis;
	
	UserInput::InputBeingSunk = m_imGuiIO->WantCaptureKeyboard;

	if (!UserInput::InputBeingSunk)
	{
		float DisplacementSpeed = MovementSpeed * DeltaTime;

		if (UserInput::IsKeyDown(SDLK_LSHIFT))
			DisplacementSpeed *= 0.5f;

		Vector3 Displacement = Vector3();

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

		Camera->Matrix = glm::translate(Camera->Matrix, glm::vec3(Displacement));
	}

	int mouseX;
	int mouseY;

	SDL_Window* window = Engine->Window;

	uint32_t activeMouseButton = SDL_GetMouseState(&mouseX, &mouseY);

	IsInputBeingSunk = m_imGuiIO->WantCaptureKeyboard || m_imGuiIO->WantCaptureMouse;

	if (MouseCaptured)
	{
		// Doesn't hide unless we tell ImGui to hide the cursor too
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

		float RotationX = MouseSensitivity * ((double)deltaMouseY - (windowSizeY / 2.0f)) / windowSizeY;
		float RotationY = MouseSensitivity * ((double)deltaMouseX - (windowSizeX / 2.0f)) / windowSizeX;
		RotationX += 50.f; // TODO 22/08/2024: Why??
		RotationY += 50.f;

		glm::vec3 newForward = glm::rotate(
			CamForward,
			glm::radians(RotationX),
			glm::normalize(glm::cross(CamForward, UpVec))
		);

		if (abs(glm::angle(newForward, UpVec) - glm::radians(90.0f)) <= glm::radians(85.0f))
			CamForward = newForward;

		CamForward = glm::rotate(CamForward, glm::radians(-RotationY), UpVec);

		glm::vec3 Position = glm::vec3(Camera->Matrix[3]);

		Camera->Matrix = glm::lookAt(Position, Position + CamForward, UpVec);

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
			Engine->SetIsFullscreen(!Engine->IsFullscreen);
		}
	else
		PreviouslyPressingF11 = false;
}

static char* LevelPathBuf;

static void LoadLevel(const std::string& LevelPath)
{
	GameObject* workspace = GameObject::s_DataModel->GetChild("Workspace");

	GameObject* prevModel = workspace->GetChild("LevelModel");

	if (prevModel)
		delete prevModel;

	GameObject* levelModel = GameObject::CreateGameObject("Model");
	levelModel->Name = "Level";
	levelModel->SetParent(workspace);

	MapLoader::LoadMapIntoObject(LevelPath, levelModel);
}

static void DrawUI(std::tuple<EngineObject*, double, double> Data)
{
	EngineObject* Engine = std::get<0>(Data);

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
		Debug::Log("Dumping GameObject API...\n");

		auto dump = GameObject::DumpApiToJson();
		FileRW::WriteFile("apidump.json", dump.dump(2), false);

		Debug::Log("API dump finished");
	}

	if (EngineJsonConfig["developer"] == true)
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Load level");

		ImGui::InputText("Load level", LevelPathBuf, 64);

		if (ImGui::Button("Load"))
			LoadLevel(LevelPathBuf);

		ImGui::End();

		ImGui::Begin("Info");

		ImGui::Text("FPS: %d", Engine->FramesPerSecond);
		ImGui::Text("Frame time: %dms", (int)ceil(Engine->FrameTime * 1000));

		ImGui::End();

		ImGui::Begin("Settings");

		ImGui::Checkbox("VSync", &Engine->VSync);

		bool WasFullscreen = Engine->IsFullscreen;

		ImGui::Checkbox("Fullscreen", &Engine->IsFullscreen);

		if (Engine->IsFullscreen != WasFullscreen)
			Engine->SetIsFullscreen(Engine->IsFullscreen);

		if (Engine->VSync)
			SDL_GL_SetSwapInterval(1);
		else
		{
			SDL_GL_SetSwapInterval(0);

			ImGui::InputInt("FPS max", &Engine->FpsCap, 1, 30);
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
				float SampleRadius = EngineJsonConfig.value("postfx_blurvignette_sampleradius", 4);

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
	EngineObject* Engine = EngineObject::Get();

	EditorContext = EngineJsonConfig.value("developer", false) ? new Editor() : nullptr;
	
	LevelPathBuf = (char*)malloc(64);

	if (!LevelPathBuf)
		throw("Could not allocate buffer for Level Path");

	for (int i = 0; i < 64; i++)
		LevelPathBuf[i] = '\0';

	const char* mapFileFromArgs;
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

	srand(time(0));

	LoadLevel(mapFile);

	if (EditorContext)
		EditorContext->Init();

	Engine->OnFrameStart.Connect(HandleInputs);
	Engine->OnFrameRenderGui.Connect(DrawUI);

	Engine->Start();

	delete Engine;
}

static void HandleCrash(SDL_Window* Window, std::string Error)
{
	auto fmtArgs = std::make_format_args(
		Error,
		"If this is the first time this has happened, please re-try. Otherwise, contact the developers."
	);

	Debug::Log(std::vformat("CRASH: {}", fmtArgs));

	std::string errMessage = std::vformat(
		"An unexpected error occurred, and the application will now close. Error details:\n\n{}\n\n{}",
		fmtArgs
	);

	SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_ERROR,
		"Engine error",
		errMessage.c_str(),
		Window
	);
}

int main(int argc, char** argv)
{
	SDL_Window* Window;

	try
	{
		EngineObject* Engine = EngineObject::Get(Vector2(1280, 720), &Window);
		//Engine->MSAASamples = 2;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		m_imGuiIO = &ImGui::GetIO();
		(void)m_imGuiIO;
		ImGui::StyleColorsDark();
		ImGui_ImplSDL2_InitForOpenGL(Window, Engine->RendererContext->GLContext);
		ImGui_ImplOpenGL3_Init("#version 460");

		Application(argc, argv);
	}
	catch (std::string Error)
	{
		HandleCrash(Window, Error);
	}
	catch (const char* Error)
	{
		HandleCrash(Window, Error);
	}
	
	Debug::Log("Application closing...");

	Debug::Save();
}
