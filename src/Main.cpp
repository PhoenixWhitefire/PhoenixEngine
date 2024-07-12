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

#include"Engine.hpp"
#include"UserInput.hpp"
#include"editor/editor.hpp"
#include"Debug.hpp"
#include"FileRW.hpp"

static bool FirstDragFrame = false;

static bool PreviouslyPressingF11 = false;
static bool PreviouslyPressingP = false;
static bool IsInputBeingSunk = false;

static ImGuiIO* m_imGuiIO = nullptr;

static const float MouseSensitivity = 100.0f;
static const float MovementSpeed = 15.f;

static Editor* EditorContext = nullptr;

static int PrevMouseX, PrevMouseY = 0;

static int FindArgumentInCliArgs(int ArgCount, char** Arguments, const char* SeekingArgument)
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
		EditorContext->Update(DeltaTime, Engine->Game->GetSceneCamera()->Matrix);

	//std::shared_ptr<Object_Base3D> crow = std::dynamic_pointer_cast<Object_Base3D>(Engine->Game->GetChild("crow"));

	//crow->Matrix = glm::translate(glm::mat4(1.0f), 20.f * glm::vec3(0.5f, 0.5f, 0.5f));

	std::shared_ptr<Object_Camera> Camera = Engine->Game->GetSceneCamera();

	glm::vec3 ForwardVec = glm::vec3(Camera->Matrix[0][2], Camera->Matrix[1][2], Camera->Matrix[2][2]);
	glm::vec3 UpVec = Vector3::UP;
	
	UserInput::InputBeingSunk = m_imGuiIO->WantCaptureKeyboard;

	if (!UserInput::InputBeingSunk)
	{
		float DisplacementSpeed = MovementSpeed * DeltaTime;

		// TODO NOTE 07/07/2024 Why is horizontal movement reversed??
		if (UserInput::IsKeyDown(SDLK_LSHIFT))
			DisplacementSpeed *= 0.5f;

		Vector3 Displacement;

		if (UserInput::IsKeyDown(SDLK_w))
			Displacement += Vector3::FORWARD * DisplacementSpeed;

		if (UserInput::IsKeyDown(SDLK_a))
			Displacement += Vector3::LEFT * DisplacementSpeed;

		if (UserInput::IsKeyDown(SDLK_s))
			Displacement += Vector3::BACKWARD * DisplacementSpeed;

		if (UserInput::IsKeyDown(SDLK_d))
			Displacement += Vector3::RIGHT * DisplacementSpeed;

		if (UserInput::IsKeyDown(SDLK_q))
			Displacement += Vector3::UP * DisplacementSpeed;

		if (UserInput::IsKeyDown(SDLK_e))
			Displacement += Vector3::DOWN * DisplacementSpeed;

		Camera->Matrix = glm::translate(Camera->Matrix, glm::vec3(Displacement));
	}

	int MouseX;
	int MouseY;

	SDL_Window* Window = Engine->Window;

	uint32_t ActiveMouseButton = SDL_GetMouseState(&MouseX, &MouseY);

	if (ActiveMouseButton & SDL_BUTTON_RMASK)
	{
		//ImGui::SetMouseCursor(ImGuiMouseCursor_None);

		int DeltaMouseX = PrevMouseX - MouseX;
		int DeltaMouseY = PrevMouseY - MouseY;

		int WindowSizeX, WindowSizeY;

		SDL_GetWindowSize(Window, &WindowSizeX, &WindowSizeY);

		if (FirstDragFrame)
		{
			SDL_SetWindowMouseGrab(Window, SDL_TRUE);
			SDL_ShowCursor(SDL_DISABLE);

			SDL_WarpMouseInWindow(Window, WindowSizeX / 2, WindowSizeY / 2);

			SDL_GetMouseState(&MouseX, &MouseY);

			PrevMouseX = MouseX;
			PrevMouseY = MouseY;

			FirstDragFrame = false;
		}
		else
		{
			//SDL_WarpMouseInWindow(Engine->Window, Engine->WindowSizeX / 2, Engine->WindowSizeY / 2);

			/*SDL_GetMouseState(&MouseX, &MouseY);

			PrevMouseX = MouseX;
			PrevMouseY = MouseY;*/
		}

		float RotationX = -MouseSensitivity * ((double)DeltaMouseY - (WindowSizeY / 2.0f)) / WindowSizeY;
		float RotationY = -MouseSensitivity * ((double)DeltaMouseX - (WindowSizeX / 2.0f)) / WindowSizeX;

		glm::vec3 NewOrientation = glm::rotate(
			ForwardVec,
			glm::radians(-RotationX),
			glm::normalize(glm::cross(ForwardVec, UpVec))
		);

		if (abs(glm::angle(NewOrientation, UpVec) - glm::radians(90.0f)) <= glm::radians(85.0f))
			ForwardVec = NewOrientation;

		ForwardVec = glm::rotate(ForwardVec, glm::radians(-RotationY), UpVec);

		glm::vec3 Position = glm::vec3(Camera->Matrix[3]);

		Camera->Matrix = glm::lookAt(Position, Position + ForwardVec, UpVec);

		// Keep the mouse in the window.
		// Teleport it to the other side if it hits the edge.

		if (MouseX <= 10)
			MouseX = WindowSizeX - 20;

		if (MouseX >= WindowSizeX - 10)
			MouseX = 20;

		if (MouseY <= 10)
			MouseY = WindowSizeY - 20;

		if (MouseY >= WindowSizeY - 10)
			MouseY = 20;

		SDL_WarpMouseInWindow(Window, MouseX, MouseY);
	}
	else
	{
		//ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
		SDL_SetWindowMouseGrab(Window, SDL_FALSE);
		SDL_ShowCursor(SDL_ENABLE);

		FirstDragFrame = true;
	}

	PrevMouseX = MouseX;
	PrevMouseY = MouseY;

	if (UserInput::IsKeyDown(SDLK_F11))
	{
		if (!PreviouslyPressingF11)
		{
			PreviouslyPressingF11 = true;

			Engine->SetIsFullscreen(!Engine->IsFullscreen);
		}
	}
	else
		PreviouslyPressingF11 = false;
}

static char* LevelPathBuf;

static void LoadLevel(const char* LevelPath, std::shared_ptr<GameObject> Into)
{
	EngineObject* Engine = EngineObject::Get();

	for (int i = 0; i < Into->GetChildren().size(); i++)
		Into->GetChildren()[i]->Enabled = false; // TODO: GameObject->~GameObject crashes :(

	MapLoader::LoadMapIntoObject(LevelPath, Into);
}

std::shared_ptr<GameObject> LevelModel = nullptr;

static void DrawUI(std::tuple<EngineObject*, double, double> Data)
{
	EngineObject* Engine = std::get<0>(Data);

	// Draw UI
	// We want it to appear on top of everything, so it needs to be drawn at the end

	if (Engine->GameConfig["developer"] == true)
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Load level");

		ImGui::InputText("Load level", LevelPathBuf, 64);

		if (ImGui::Button("Load"))
			LoadLevel(LevelPathBuf, LevelModel);

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
			FileRW::WriteFile("phoenix.conf", EngineJsonConfig.dump(), false);
			Debug::Log("The JSON Config overwrote the pre-existing 'phoenix.conf'.");
		}

		ImGui::End();

		EditorContext->RenderUI();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	if (m_imGuiIO->WantCaptureKeyboard || m_imGuiIO->WantCaptureMouse)
		IsInputBeingSunk = true;
	else
		IsInputBeingSunk = false;
}

static void setChildrenComputePhysicsTrue(std::shared_ptr<GameObject> obj)
{
	for (uint32_t objidx = 0; objidx < obj->GetChildren().size(); objidx++)
	{
		std::shared_ptr<Object_Base3D> obj3d = std::dynamic_pointer_cast<Object_Base3D>(obj->GetChildren()[objidx]);

		if (obj3d)
			obj3d->ComputePhysics = true;

		if (obj->GetChildren()[objidx]->GetChildren().size() > 0)
			setChildrenComputePhysicsTrue(obj->GetChildren()[objidx]);
	}
}

static void Application(int argc, char** argv)
{
	EngineObject* Main = EngineObject::Get();

	EditorContext = EngineJsonConfig.value("developer", false) ? new Editor() : nullptr;
	
	LevelPathBuf = (char*)malloc(64);

	if (!LevelPathBuf)
	{
		throw(std::string("WHY IS LevelPathBuf NULL?!"));
		return;
	}

	for (int i = 0; i < 64; i++)
		LevelPathBuf[i] = 'a';

	LevelPathBuf[63] = '\0';

	const char* MapFileFromArgs;
	bool hasMapFromArgs = false;

	bool forceAllObjectPhysics = false;

	int MapFileArgIdx = FindArgumentInCliArgs(argc, argv, "-loadmap");
	int ForceAllObjectPhysicsArgIdx = FindArgumentInCliArgs(argc, argv, "-phys_forceallobjectphysics");

	if (MapFileArgIdx > 0)
	{
		Debug::Log(std::vformat(
			"Map to load specified from launch argument. Map was: {}",
			std::make_format_args(argv[MapFileArgIdx + 1])
		));

		MapFileFromArgs = argv[MapFileArgIdx + 1];
		hasMapFromArgs = true;
	}

	if (ForceAllObjectPhysicsArgIdx > 0)
		forceAllObjectPhysics = true;

	const char* MapFile = hasMapFromArgs ? MapFileFromArgs : "levels/dev_fmtv2.world";

	srand(time(0));

	SDL_DisplayMode Mode;
	SDL_GetCurrentDisplayMode(0, &Mode);

	LevelModel = GameObjectFactory::CreateGameObject("Model");

	/*
	if (forceAllObjectPhysics)
	{
		Debug::Log("FORCE ALL OBJECT PHYSICS IS ENABLED! (through -phys_forceallobjectphysics)");

		setChildrenComputePhysicsTrue(Main->Game);
	}
	*/

	LevelModel->SetParent(EngineObject::Get()->Game);
	LevelModel->Name = "LoadedLevel";

	LoadLevel(MapFile, LevelModel);

	if (EditorContext)
		EditorContext->Init(Main->Game);

	Main->OnFrameStart.Connect(HandleInputs);
	Main->OnFrameRenderGui.Connect(DrawUI);

	Main->Start();
}

int main(int argc, char** argv)
{
	EngineObject* Engine = nullptr;

	SDL_Window* Window;

	try
	{
		SDL_DisplayMode Mode;
		SDL_GetCurrentDisplayMode(0, &Mode);

		Engine = EngineObject::Get(Vector2(1280, 720), &Window);
		//Engine->MSAASamples = 2;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		m_imGuiIO = &ImGui::GetIO();
		(void)m_imGuiIO;
		ImGui::StyleColorsDark();
		ImGui_ImplSDL2_InitForOpenGL(Engine->Window, Engine->m_renderer->m_glContext);
		ImGui_ImplOpenGL3_Init("#version 460");

		Application(argc, argv);
	}
	catch (std::string Error)
	{
		Debug::Log(std::vformat("CRASH: {}", std::make_format_args(Error)));

		auto fmtArgs = std::make_format_args(Error);

		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			"Engine error",
			std::vformat(
				"An unexpected error occurred, and the application will now close. Error details (S):\n\n{}",
				std::make_format_args(Error)
			).c_str(),
			Window
		);
	}
	catch (const char* Error)
	{
		Debug::Log(std::vformat("CRASH: {}", std::make_format_args(Error)));

		auto fmtArgs = std::make_format_args(Error);

		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			"Engine error",
			std::vformat(
				"An unexpected error occurred, and the application will now close. Error details (C):\n\n{}",
				std::make_format_args(Error)
			).c_str(),
			Window
		);
	}
	
	Debug::Save();
	
	return EXIT_SUCCESS;
}

