/*

Phoenix Engine

The oldest files I can find show that I started working on this on the 2nd of November, 2021.
Today, it is the 5th of March, 2023

This is a melting, sphaghetti code hodgepodge of random things I thought was cool when I was writing it
that SOMEHOW works without crashing atleast 50 times a frame.

Anyway, here is a small tour:

- All shader files have a file extension of one of the following: .vert, .frag, .geom
- The "main" shaders are the two worldUber.vert and worldUber.frag ones
- "phoenix.conf" contains some configuration. Set "developer" to "false" to disable the debug UIs.
- Hold R to disable distance culling
- WASD to move horizontally, Q/E to move down/up. Right-click and drag to look. LShift to move slower.
- F11 to toggle fullscreen.

*/

#define SDL_MAIN_HANDLED

#include"editor/editor.hpp"

#include<gameobject/GameObjects.hpp>

#include<datatype/Vector3.hpp>

#include<nljson.h>

#include<Engine.hpp>
#include<UserInput.hpp>

#include<iostream>
#include<math.h>
#include<cmath>

#include<Windows.h>

#include<imgui/imgui_impl_opengl3.h>
#include<imgui/imgui_impl_sdl.h>

static bool FirstClick = false;

static bool PreviouslyPressingF11 = false;
static bool PreviouslyPressingP = false;
static bool IsInputBeingSunk = false;

ImGuiIO* m_imGuiIO = nullptr;

static double CameraSpeed = 0.5f;
static float MouseSensitivity = 100.0f;

Editor* EditorContext = nullptr;

int PrevMouseX, PrevMouseY = 0;

int FindArgumentInCliArgs(int ArgCount, char** Arguments, const char* SeekingArgument)
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

void HandleInputs(std::tuple<EngineObject*, double, double> Data)
{

	EngineObject* Engine = std::get<0>(Data);
	double Delta = std::get<1>(Data);
	double CurrentTime = std::get<2>(Data);

	Camera* Player = Engine->MainCamera;

	EditorContext->Update(Delta, Player->Matrix);

	//std::shared_ptr<Object_Base3D> crow = std::dynamic_pointer_cast<Object_Base3D>(Engine->Game->GetChild("crow"));

	//crow->Matrix = glm::translate(glm::mat4(1.0f), 20.f * glm::vec3(0.5f, 0.5f, 0.5f));

	if (!IsInputBeingSunk)
	{
		float Displacement = CameraSpeed * Delta;

		if (UserInput::IsKeyDown(SDLK_LSHIFT))
			Displacement = CameraSpeed * 0.2f;
		else
			Displacement = CameraSpeed;

		if (UserInput::IsKeyDown(SDLK_w))
			Player->Position += Player->LookVec * Displacement;

		if (UserInput::IsKeyDown(SDLK_a))
			Player->Position += -glm::normalize(glm::cross(Player->LookVec, Player->UpVec)) * Displacement;

		if (UserInput::IsKeyDown(SDLK_s))
			Player->Position += -Player->LookVec * Displacement;

		if (UserInput::IsKeyDown(SDLK_d))
			Player->Position += glm::normalize(glm::cross(Player->LookVec, Player->UpVec)) * Displacement;

		if (UserInput::IsKeyDown(SDLK_q))
			Player->Position += glm::vec3(0.f, -Displacement, 0.f);

		if (UserInput::IsKeyDown(SDLK_e))
			Player->Position += glm::vec3(0.f, Displacement, 0.f);
	}
	
	int MouseX;
	int MouseY;

	uint32_t ActiveMouseButton = SDL_GetMouseState(&MouseX, &MouseY);

	if (ActiveMouseButton & SDL_BUTTON_RMASK)
	{
		//ImGui::SetMouseCursor(ImGuiMouseCursor_None);

		int DeltaMouseX = PrevMouseX - MouseX;
		int DeltaMouseY = PrevMouseY - MouseY;

		if (FirstClick)
		{
			SDL_WarpMouseInWindow(Engine->Window, Engine->WindowSizeX / 2, Engine->WindowSizeY / 2);
			SDL_SetWindowMouseGrab(Engine->Window, SDL_TRUE);
			SDL_ShowCursor(SDL_DISABLE);

			SDL_GetMouseState(&MouseX, &MouseY);

			PrevMouseX = MouseX;
			PrevMouseY = MouseY;

			FirstClick = false;
		}
		else
		{
			//SDL_WarpMouseInWindow(Engine->Window, Engine->WindowSizeX / 2, Engine->WindowSizeY / 2);

			/*SDL_GetMouseState(&MouseX, &MouseY);

			PrevMouseX = MouseX;
			PrevMouseY = MouseY;*/
		}

		float RotationX = -MouseSensitivity * ((double)DeltaMouseY - (Engine->WindowSizeY / 2.0f)) / Engine->WindowSizeY;
		float RotationY = -MouseSensitivity * ((double)DeltaMouseX - (Engine->WindowSizeX / 2.0f)) / Engine->WindowSizeX;

		// TODO: ??? 28/04/2024
		RotationX -= MouseSensitivity / 2.f;
		RotationY -= MouseSensitivity / 2.f;

		//printf("RX: %f RY: %f\n", RotationX, RotationY);

		glm::vec3 NewOrientation = glm::rotate(
			Player->LookVec,
			glm::radians(-RotationX),
			glm::normalize(glm::cross(Player->LookVec, Player->UpVec))
		);

		if (abs(glm::angle(NewOrientation, Player->UpVec) - glm::radians(90.0f)) <= glm::radians(85.0f)) {
			Player->LookVec = NewOrientation;
		}

		Player->LookVec = glm::rotate(Player->LookVec, glm::radians(-RotationY), Player->UpVec);

		// Keep the mouse in the window.
		// Teleport it to the other side if it hits the edge.
		int ResX, ResY;

		SDL_GetWindowSize(SDL_GetGrabbedWindow(), &ResX, &ResY);

		if (MouseX <= 10)
			MouseX = ResX - 20;

		if (MouseX >= ResX - 10)
			MouseX = 20;

		if (MouseY <= 10)
			MouseY = ResY - 20;

		if (MouseY >= ResY - 10)
			MouseY = 20;

		SDL_WarpMouseInWindow(SDL_GetGrabbedWindow(), MouseX, MouseY);
	}
	else
	{
		//ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
		SDL_SetWindowMouseGrab(Engine->Window, SDL_FALSE);
		SDL_ShowCursor(SDL_ENABLE);

		FirstClick = true;
	}

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

	PrevMouseX = MouseX;
	PrevMouseY = MouseY;
}

static char* LevelPathBuf;

void LoadLevel(const char* LevelPath, std::shared_ptr<GameObject> Into)
{
	EngineObject* Engine = EngineObject::Get();

	for (int i = 0; i < Into->GetChildren().size(); i++)
		Into->GetChildren()[i]->Enabled = false; // TODO: GameObject->~GameObject crashes :(

	MapLoader::LoadMapIntoObject(LevelPath, Into);

	Debug::Log(std::vformat("Loaded level: {}", std::make_format_args(LevelPath)));
}

std::shared_ptr<GameObject> LevelModel = nullptr;

void DrawUI(std::tuple<EngineObject*, double, double> Data)
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
		}

		ImGui::End();

		EditorContext->RenderUI();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	if (m_imGuiIO->WantTextInput)
		IsInputBeingSunk = true;
	else
		IsInputBeingSunk = false;
}

void setChildrenComputePhysicsTrue(std::shared_ptr<GameObject> obj)
{
	for (unsigned int objidx = 0; objidx < obj->GetChildren().size(); objidx++)
	{
		std::shared_ptr<Object_Base3D> obj3d = std::dynamic_pointer_cast<Object_Base3D>(obj->GetChildren()[objidx]);

		if (obj3d)
			obj3d->ComputePhysics = true;

		if (obj->GetChildren()[objidx]->GetChildren().size() > 0)
			setChildrenComputePhysicsTrue(obj->GetChildren()[objidx]);
	}
}

void Application(int argc, char** argv)
{
	EngineObject* Main = EngineObject::Get();

	EditorContext = new Editor();
	
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
		printf("-loadmap was arg num %d\n", MapFileArgIdx);

		MapFileFromArgs = argv[MapFileArgIdx + 1];
		hasMapFromArgs = true;
	}

	if (ForceAllObjectPhysicsArgIdx > 0)
		forceAllObjectPhysics = true;

	const char* MapFile = hasMapFromArgs ? MapFileFromArgs : "levels/dev.world";

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
	LevelModel->ParentLocked = true;

	LoadLevel(MapFile, LevelModel);

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
			std::vformat("An error occurred, and the application needs to close! Error details:\n\n{}", fmtArgs).c_str(),
			Window
		);
	}
	
	Debug::Save();
	
	return EXIT_SUCCESS;
}

