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
- Press `L` to dump GameObject API.
- `K` to reload all shaders, `I` to reload configuration
- F11 to toggle fullscreen.

https://github.com/Phoenixwhitefire/PhoenixEngine

*/

/*
	
	14/11/2024

	This Main file is in the "Player" or "Application" layer in my head, so I don't really care
	that's it's messy.

*/

#define GLM_ENABLE_EXPERIMENTAL
#define SDL_MAIN_HANDLED

#define PHX_MAIN_HANDLECRASH(c, expr) catch (c Error) { handleCrash(Error##expr, #c); }

#define PHX_MAIN_CRASHHANDLERS PHX_MAIN_HANDLECRASH(std::string, ) \
PHX_MAIN_HANDLECRASH(const char*, ) \
PHX_MAIN_HANDLECRASH(std::bad_alloc, .what() + std::string(": System may have run out of memory")) \
PHX_MAIN_HANDLECRASH(nlohmann::json::type_error, .what()) \
PHX_MAIN_HANDLECRASH(nlohmann::json::parse_error, .what()) \
PHX_MAIN_HANDLECRASH(std::exception, .what()); \

#include <ImGuiFD/ImGuiFD.h>

#include <filesystem>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "Engine.hpp"

#include "GlobalJsonConfig.hpp"
#include "UserInput.hpp"
#include "Utilities.hpp"
#include "Profiler.hpp"
#include "FileRW.hpp"
#include "Editor.hpp"
#include "Log.hpp"
#include "asset/SceneFormat.hpp"

static bool FirstDragFrame = false;

static bool PreviouslyPressingF11 = false;
static bool MouseCaptured = false;

static ImGuiIO* GuiIO = nullptr;

static const float MouseSensitivity = 100.0f;
static const float MovementSpeed = 15.f;

static Editor* EditorContext = nullptr;
static EngineObject* EngineInstance = nullptr;

static int PrevMouseX, PrevMouseY = 0;

static glm::tvec3<double, glm::highp> CamForward = glm::vec3(0.f, 0.f, -1.f);

// 16/11/2024
// https://stackoverflow.com/a/5167641/16875161
static std::vector<std::string> stringSplit(const std::string& s, const std::string& seperator)
{
	if (s.find(seperator) == std::string::npos)
		return { s };

	std::vector<std::string> output;

	std::string::size_type prev_pos = 0, pos = 0;

	while ((pos = s.find(seperator, pos)) != std::string::npos)
	{
		std::string substring(s.substr(prev_pos, pos - prev_pos));

		output.push_back(substring);

		prev_pos = ++pos;
	}

	output.push_back(s.substr(prev_pos, pos - prev_pos)); // Last word

	return output;
}

static int findArgumentInCliArgs(
	int ArgCount,
	char** Arguments,
	const char* SeekingArgument
)
{
	// linear search through all cli arguments
	for (int argIndex = 0; argIndex < ArgCount; argIndex++)
	{
		// strcmp makes no sense, why does it return 0 if they match and not 1??
		if (strcmp(Arguments[argIndex], SeekingArgument) == 0)
			return argIndex;
	}

	// argument not found
	return 0;
}

static void handleInputs(Reflection::GenericValue Data)
{
	double deltaTime = Data.AsDouble();

	if (EngineJsonConfig.value("Developer", false))
		EditorContext->Update(deltaTime);

	Object_Camera* camera = EngineInstance->Workspace->GetSceneCamera();
	SDL_Window* window = EngineInstance->Window;

	UserInput::InputBeingSunk = GuiIO->WantCaptureKeyboard || GuiIO->WantCaptureMouse;

	if (camera->UseSimpleController)
	{
		static const glm::tvec3<double, glm::highp> UpVec = Vector3::yAxis;

		if (!UserInput::InputBeingSunk)
		{
			double displacementSpeed = MovementSpeed * deltaTime;

			if (UserInput::IsKeyDown(SDLK_LSHIFT))
				displacementSpeed *= 0.5f;

			Vector3 displacement{};

			if (UserInput::IsKeyDown(SDLK_w))
				displacement += Vector3(0, 0, displacementSpeed);

			if (UserInput::IsKeyDown(SDLK_a))
				displacement += Vector3(displacementSpeed, 0, 0);

			if (UserInput::IsKeyDown(SDLK_s))
				displacement += Vector3(0, 0, -displacementSpeed);

			if (UserInput::IsKeyDown(SDLK_d))
				displacement += Vector3(-displacementSpeed, 0, 0);

			if (UserInput::IsKeyDown(SDLK_q))
				displacement += Vector3(0, -displacementSpeed, 0);

			if (UserInput::IsKeyDown(SDLK_e))
				displacement += Vector3(0, displacementSpeed, 0);

			camera->Transform = glm::translate(camera->Transform, (glm::vec3)displacement);
		}

		int mouseX;
		int mouseY;

		uint32_t activeMouseButton = SDL_GetMouseState(&mouseX, &mouseY);

		if (MouseCaptured)
		{
			// Doesn't hide unless we tell Dear ImGui to hide the cursor too
			// (Otherwise it flickers)
			// 22/08/2024
			ImGui::SetMouseCursor(ImGuiMouseCursor_None);
			PHX_SDL_CALL(SDL_ShowCursor, SDL_DISABLE);

			int windowSizeX, windowSizeY;

			SDL_GetWindowSize(nullptr, &windowSizeX, &windowSizeY);

			if (FirstDragFrame)
			{
				SDL_SetWindowMouseGrab(window, SDL_TRUE);

				SDL_WarpMouseInWindow(nullptr, windowSizeX / 2, windowSizeY / 2);

				mouseX, mouseY = windowSizeX / 2, windowSizeY / 2;
				PrevMouseX, PrevMouseY = mouseX, mouseY;

				FirstDragFrame = false;
			}

			int deltaMouseX = PrevMouseX - mouseX;
			int deltaMouseY = PrevMouseY - mouseY;

			double rotationX = MouseSensitivity * ((double)deltaMouseY - (windowSizeY / 2.0f)) / windowSizeY;
			double rotationY = MouseSensitivity * ((double)deltaMouseX - (windowSizeX / 2.0f)) / windowSizeX;
			rotationX += 50.f; // TODO 22/08/2024: Why??
			rotationY += 50.f;

			glm::tvec3<double, glm::highp> newForward = glm::rotate(
				CamForward,
				glm::radians(rotationX),
				glm::normalize(glm::cross(CamForward, UpVec))
			);

			if (abs(glm::angle(newForward, UpVec) - glm::radians(90.0f)) <= glm::radians(85.0f))
				CamForward = newForward;

			CamForward = glm::rotate(CamForward, glm::radians(-rotationY), UpVec);

			glm::tvec3<double, glm::highp> position{ camera->Transform[3] };

			camera->Transform = glm::lookAt(position, position + CamForward, UpVec);

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

			if (UserInput::InputBeingSunk)
				newMouseX, newMouseY = windowSizeX / 2, windowSizeY / 2;

			if (newMouseX != mouseX || newMouseY != mouseY)
			{
				SDL_WarpMouseInWindow(nullptr, newMouseX, newMouseY);
				mouseX = newMouseX;
				mouseY = newMouseY;
			}

			if (activeMouseButton & SDL_BUTTON_RMASK)
			{
				MouseCaptured = false;
				SDL_WarpMouseInWindow(nullptr, windowSizeX / 2, windowSizeY / 2);
			}
		}
		else
		{
			if (!FirstDragFrame)
			{
				PHX_SDL_CALL(SDL_ShowCursor, SDL_ENABLE);
				SDL_SetWindowMouseGrab(window, SDL_FALSE);

				ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

				FirstDragFrame = true;
			}

			//if (activeMouseButton & SDL_BUTTON_LMASK && !UserInput::InputBeingSunk)
				//MouseCaptured = true;
		}

		PrevMouseX = mouseX;
		PrevMouseY = mouseY;
	}
	else
	{
		// `input_mouse_setlocked` Luau API 21/09/2024
		if (Object_Script::s_WindowGrabMouse && !UserInput::InputBeingSunk)
		{
			SDL_SetWindowMouseGrab(window, SDL_TRUE);

			PHX_SDL_CALL(SDL_ShowCursor, SDL_DISABLE);
			ImGui::SetMouseCursor(ImGuiMouseCursor_None);

			int mouseX = 0, mouseY = 0;

			SDL_GetMouseState(&mouseX, &mouseY);

			// Keep the mouse in the window.
			// Teleport it to the other side if it hits the edge.

			int newMouseX = mouseX, newMouseY = mouseY;

			if (mouseX <= 10)
				newMouseX = EngineInstance->WindowSizeX - 20;

			if (mouseX >= EngineInstance->WindowSizeX - 10)
				newMouseX = 20;

			if (mouseY <= 10)
				newMouseY = EngineInstance->WindowSizeY - 20;

			if (mouseY >= EngineInstance->WindowSizeY - 10)
				newMouseY = 20;

			if (UserInput::InputBeingSunk)
				newMouseX, newMouseY = EngineInstance->WindowSizeX / 2, EngineInstance->WindowSizeY / 2;

			if (newMouseX != mouseX || newMouseY != mouseY)
			{
				SDL_WarpMouseInWindow(window, newMouseX, newMouseY);
				mouseX = newMouseX;
				mouseY = newMouseY;
			}
		}
		else
		{
			SDL_SetWindowMouseGrab(window, SDL_FALSE);

			PHX_SDL_CALL(SDL_ShowCursor, SDL_ENABLE);
			ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
		}
	}

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
	Log::Info(std::vformat("Loading scene: '{}'", std::make_format_args(LevelPath)));

	Object_Workspace* workspace = EngineInstance->Workspace;

	GameObject* prevModel = workspace->GetChild("Level");

	if (prevModel)
		prevModel->Destroy();

	GameObject* levelModel = GameObject::Create("Model");
	levelModel->Name = "Level";
	levelModel->SetParent(workspace);

	bool loadSuccess = true;

	// 11/09/2024
	// Today marks the day a Luau script can move the Camera
	// It'll set `UseSimpleController` to false so that `handleInputs` doesn't
	// mess with it
	// We set it to true here in case the next level does not have a Camera Control Script
	// (cause I still want to look around :3)
	// (without having to go to the Camera in the Hierarchy and manually re-enable it :3)
	workspace->GetSceneCamera()->UseSimpleController = true;
	// 21/09/2024
	// Also reset the cam back to the origin because !!PHYSICS!! yay
	// so if we fall down into the *VOID* we get sent back up
	workspace->GetSceneCamera()->Transform = glm::mat4(1.f);

	std::vector<GameObject*> objects = SceneFormat::Deserialize(FileRW::ReadFile(LevelPath), &loadSuccess);

	if (loadSuccess)
	{
		for (GameObject* object : objects)
			object->SetParent(levelModel);
	}
	else
		throw("Failed to load level: " + SceneFormat::GetLastErrorString());

	//MapLoader::LoadMapIntoObject(LevelPath, levelModel);
}

static double recurseGetTime(const nlohmann::json& root)
{
	if (root.find("_t") != root.end())
		return root["_t"];

	double t{};

	for (auto ch = root.begin(); ch != root.end(); ++ch)
		t += recurseGetTime(ch.value());

	return t;
}

static void recurseProfilerUI(const nlohmann::json& tree)
{
	for (auto it = tree.begin(); it != tree.end(); ++it)
	{
		auto& v = it.value();

		if (v.type() != nlohmann::json::value_t::object)
			continue;

		// "EventCallbacks" doesn't have an `_t`, accumulate the timings of
		// it's children
		// 08/12/2024
		double t = recurseGetTime(v);

		uint32_t tMS = static_cast<uint32_t>(std::floor(t * 100000.f));
		float tMSHundreds = tMS / 100.f;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;

		if (it.value().size() == 1)
			flags |= ImGuiTreeNodeFlags_Leaf;

		bool open = ImGui::TreeNodeEx(
			it.key().c_str(),
			flags,
			(std::vformat(
				"{}: {}ms",
				std::make_format_args(it.key(), tMSHundreds)
			)).c_str()
		);

		if (open)
		{
			recurseProfilerUI(it.value());
			ImGui::TreePop();
		}
	}
}

static void drawUI(Reflection::GenericValue Data)
{
	if (UserInput::IsKeyDown(SDLK_l) && !UserInput::InputBeingSunk)
	{
		Log::Info("Dumping GameObject API...");

		auto dump = GameObject::DumpApiToJson();
		FileRW::WriteFile("apidump.json", dump.dump(2), false);

		Log::Info("API dump finished");
	}

	if (UserInput::IsKeyDown(SDLK_i) && !UserInput::InputBeingSunk)
	{
		Log::Info("Reloading configuration...");

		EngineInstance->LoadConfiguration();
	}

	if (UserInput::IsKeyDown(SDLK_k) && !UserInput::InputBeingSunk)
	{
		Log::Info("Reloading shaders...");

		ShaderManager::Get()->ReloadAll();

		Log::Info("Shaders reloaded");
	}

	if (EditorContext)
	{
		//ImGui_ImplOpenGL3_NewFrame();
		//ImGui_ImplSDL2_NewFrame();
		//ImGui::NewFrame();

		//ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

		if (ImGui::Begin("Level"))
		{
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

					if (alreadyExists && prevFile.find("#Version 2.00") == std::string::npos)
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

		}
		ImGui::End();

		// always pop the snapshot, otherwise they'll build-up
		std::unordered_map<std::string, double> snapshot = Profiler::PopSnapshot();

		static nlohmann::json ProfileInfoHisto[100] = {};
		static float FrameTimesHisto[100] = { 0 };

		if (ImGui::Begin("Info") && snapshot.size() > 0)
		{
			ImGui::Text("FPS: %d", EngineInstance->FramesPerSecond);
			ImGui::Text("Frame time: %dms", (int)std::ceil(EngineInstance->FrameTime * 1000));
			ImGui::Text("Draw calls: %zi", EngineJsonConfig.value("renderer_drawcallcount", 0ull));

			ImGui::Text("--- PROFILING ---");

			static bool InfoCollectionPaused = true;

			nlohmann::json profilerTimingsTree{};

			if (!InfoCollectionPaused)
				for (auto& it : snapshot)
				{
					nlohmann::json* last = &profilerTimingsTree;

					std::vector<std::string> split = stringSplit(it.first, "/");

					for (size_t catIndex = 0; catIndex < split.size(); catIndex++)
						if (catIndex < split.size() - 1)
						{
							last = &((*last)[split[catIndex]]);
							//(*last)["_t"] = it.second;
						}
						else
							(*last)[split[catIndex]]["_t"] = it.second;
				}

			if (ProfileInfoHisto->size() == 0)
				for (int i = 0; i < 100; i++)
				{
					ProfileInfoHisto[i] = nlohmann::json::object();
					ProfileInfoHisto[i]["Frame"] = nlohmann::json::object();
					ProfileInfoHisto[i]["Frame"]["_t"] = 0.f;
				}

			if (!InfoCollectionPaused)
			{
				ProfileInfoHisto[99] = profilerTimingsTree;
				FrameTimesHisto[99] = profilerTimingsTree["Frame"]["_t"];
			}

			if (!InfoCollectionPaused)
				for (int i = 0; i < 99; i++)
				{
					ProfileInfoHisto[i] = ProfileInfoHisto[i + 1];
					FrameTimesHisto[i] = FrameTimesHisto[i + 1];
				}

			static auto FrameTimeHistoPeeker = [](void* data, int idx)
				-> float
				{
					return ((float*)data)[idx];
				};

			ImGui::PlotHistogram("MS", FrameTimeHistoPeeker, FrameTimesHisto, 100, 0, 0, 1/120.f, 1/30.f);

			static int FocusedProfilingInfoIdx = 99;

			if (ImGui::IsItemClicked())
			{
				int mouseX = 0;
				SDL_GetMouseState(&mouseX, NULL);

				ImVec2 min = ImGui::GetItemRectMin() + ImGui::GetStyle().FramePadding;
				ImVec2 max = ImGui::GetItemRectMax() - ImGui::GetStyle().FramePadding;

				const float t = std::clamp((ImGui::GetIO().MousePos.x - min.x) / (max.x - min.x), 0.0f, 0.9999f);
				FocusedProfilingInfoIdx = (int)(t * 100);
			}

			if (InfoCollectionPaused && ImGui::Button("Unpause"))
				InfoCollectionPaused = false;

			else if (!InfoCollectionPaused && ImGui::Button("Pause"))
				InfoCollectionPaused = true;

			recurseProfilerUI(ProfileInfoHisto[FocusedProfilingInfoIdx]);

			if (ImGui::Button("Save"))
				ImGuiFD::OpenDialog("SaveProfilingData", ImGuiFDMode_SaveFile, "", "*.json");

			if (ImGui::Button("Load"))
				ImGuiFD::OpenDialog("LoadProfilingData", ImGuiFDMode_LoadFile, "", "*.json");

		}
		ImGui::End();

		if (ImGuiFD::BeginDialog("SaveProfilingData"))
		{
			if (ImGuiFD::ActionDone())
			{
				if (ImGuiFD::SelectionMade())
				{
					std::string result = ImGuiFD::GetResultStringRaw();
					std::string contents = "";

					if (result.find(".json") == std::string::npos)
						result += ".json";

					result = ("./" + result).c_str();

					for (int i = 0; i < 100; i++)
						contents += ProfileInfoHisto[i].dump(-1) + '\n';

					try
					{
						FileRW::WriteFile(result, contents, false);
					}
					catch (std::string err)
					{
						Log::Error("While trying to save profiling data: " + err);
					}
				}

				ImGuiFD::CloseCurrentDialog();
			}

			ImGuiFD::EndDialog();
		}

		if (ImGuiFD::BeginDialog("LoadProfilingData"))
		{
			if (ImGuiFD::ActionDone())
			{
				if (ImGuiFD::SelectionMade())
				{
					std::string result = ImGuiFD::GetSelectionPathString(0);
					bool exists = false;
					std::string contents = FileRW::ReadFile(result, &exists, false);

					if (exists)
						for (int i = 0; i < 100; i++)
						{
							size_t nextLine = contents.find_first_of('\n');

							std::string substr = contents.substr(0, nextLine);

							ProfileInfoHisto[i] = nlohmann::json::parse(substr);

							contents = contents.substr(nextLine + 1);
						}
					else
						Log::Error("Couldn't load profiling data from file " + result);
				}

				ImGuiFD::CloseCurrentDialog();
			}

			ImGuiFD::EndDialog();
		}

		if (ImGui::Begin("Settings"))
		{
			ImGui::Checkbox("VSync", &EngineInstance->VSync);

			bool wasFullscreen = EngineInstance->IsFullscreen;

			ImGui::Checkbox("Fullscreen", &EngineInstance->IsFullscreen);

			if (EngineInstance->IsFullscreen != wasFullscreen)
				EngineInstance->SetIsFullscreen(EngineInstance->IsFullscreen);

				if (EngineInstance->VSync)
					SDL_GL_SetSwapInterval(1);
				else
				{
					SDL_GL_SetSwapInterval(0);

					ImGui::InputInt("FPS max", &EngineInstance->FpsCap, 1, 30);
				}

			bool postFxEnabled = EngineJsonConfig.value("postfx_enabled", false);

			ImGui::Checkbox("Post-Processing", &postFxEnabled);

			EngineJsonConfig["postfx_enabled"] = postFxEnabled;

			if (postFxEnabled)
			{
				float gammaCorrection = EngineJsonConfig.value("postfx_gamma", 1.f);
				float trldmax = EngineJsonConfig.value("postfx_ldmax", 1.f);
				float trcmax = EngineJsonConfig.value("postfx_cmax", 1.f);

				ImGui::InputFloat("Gamma", &gammaCorrection);
				ImGui::InputFloat("Tonemapper LdMax", &trldmax);
				ImGui::InputFloat("Tonemapper CMax", &trcmax);

				EngineJsonConfig["postfx_gamma"] = gammaCorrection;
				EngineJsonConfig["postfx_ldmax"] = trldmax;
				EngineJsonConfig["postfx_cmax"] = trcmax;

				bool blurVignette = EngineJsonConfig.value("postfx_blurvignette", false);
				bool distortion = EngineJsonConfig.value("postfx_distortion", false);

				ImGui::Checkbox("Blur vignette", &blurVignette);
				ImGui::Checkbox("Distortion", &distortion);

				EngineJsonConfig["postfx_blurvignette"] = blurVignette;
				EngineJsonConfig["postfx_distortion"] = distortion;

				if (EngineJsonConfig["postfx_blurvignette"])
				{
					float distFactorMultiplier = EngineJsonConfig.value("postfx_blurvignette_blurstrength", 2.f);
					float weightExponent = EngineJsonConfig.value("postfx_blurvignette_weightexp", 2.f);
					float weightMultiplier = EngineJsonConfig.value("postfx_blurvignette_weightmul", 2.5f);
					float sampleRadius = EngineJsonConfig.value("postfx_blurvignette_sampleradius", 4.f);

					ImGui::InputFloat("Vignette dist weight factor", &distFactorMultiplier);
					ImGui::InputFloat("Vignette weight exponent", &weightExponent);
					ImGui::InputFloat("Vignette weight multiplier", &weightMultiplier);
					ImGui::InputFloat("Vignette sample radius", &sampleRadius);

					EngineJsonConfig["postfx_blurvignette_blurstrength"] = distFactorMultiplier;
					EngineJsonConfig["postfx_blurvignette_weightexp"] = weightExponent;
					EngineJsonConfig["postfx_blurvignette_weightmul"] = weightMultiplier;
					EngineJsonConfig["postfx_blurvignette_sampleradius"] = sampleRadius;
				}
			}

			if (ImGui::Button("Save Post FX settings"))
			{
				FileRW::WriteFile("phoenix.conf", EngineJsonConfig.dump(2), false);
				Log::Info("The JSON Config overwrote the pre-existing 'phoenix.conf'.");
			}
		}
		ImGui::End();

		EditorContext->RenderUI();
	}
}

static void Application(int argc, char** argv)
{
	const char* imGuiVersion = IMGUI_VERSION;

	Log::Info(std::vformat(
		"Initializing Dear ImGui {}...",
		std::make_format_args(imGuiVersion)
	));

	bool imGuiVersionCorrect = IMGUI_CHECKVERSION();

	if (!imGuiVersionCorrect)
		throw("Dear ImGui detected a version mismatch");

	ImGui::CreateContext();

	GuiIO = &ImGui::GetIO();
	//GuiIO->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::StyleColorsDark();

	if (!ImGui_ImplSDL2_InitForOpenGL(
			EngineInstance->Window,
			EngineInstance->RendererContext.GLContext
	))
		throw("ImGui intialization failure on `ImGui_ImplSDL2_InitForOpenGL`");

	if (!ImGui_ImplOpenGL3_Init("#version 460"))
		throw("ImGui initialization failure on `ImGui_ImplOpenGL3_Init`");

	EditorContext = EngineJsonConfig.value("Developer", false) ? new Editor(&EngineInstance->RendererContext) : nullptr;
	
	LevelLoadPathBuf = BufferInitialize(64, "levels/de_dust2.world");
	LevelSavePathBuf = BufferInitialize(64, "levels/save.world");

	const char* mapFileFromArgs{};
	bool hasMapFromArgs = false;

	int mapFileArgIdx = findArgumentInCliArgs(argc, argv, "-loadmap");

	if (mapFileArgIdx > 0)
	{
		Log::Info(std::vformat(
			"Map to load specified from launch argument. Map was: {}",
			std::make_format_args(argv[mapFileArgIdx + 1])
		));

		mapFileFromArgs = argv[mapFileArgIdx + 1];
		hasMapFromArgs = true;
	}

	std::string mapFile = hasMapFromArgs ?
							mapFileFromArgs
							: EngineJsonConfig.value("RootScene", "scenes/root.world");

	bool worldLoadSuccess = true;
	std::vector<GameObject*> roots = SceneFormat::Deserialize(FileRW::ReadFile(mapFile), &worldLoadSuccess);

	if (!worldLoadSuccess)
	{
		std::string errStr = SceneFormat::GetLastErrorString();
		throw(std::vformat(
			"World failed to load: {}",
			std::make_format_args(errStr)
		));
	}

	if (roots.size() > 1)
		Log::Warning("More than 1 root object in the World, anything other than the first will be ignored");

	if (roots.empty())
		throw("No root objects in World!");
	if (roots[0]->ClassName != "DataModel")
		throw("Root object was not a DataModel!");

	GameObject::s_DataModel->CallFunction("Merge", { roots[0]->ToGenericValue() });

	//LoadLevel(mapFile);

	EngineInstance->OnFrameStart.Connect(handleInputs);
	EngineInstance->OnFrameRenderGui.Connect(drawUI);

	EngineInstance->Start();

	// After the Main Loop exits

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();

	SDL_Quit();

	// Engine destructor is called as the `EngineObject`'s scope terminates in `main`.
}

static void handleCrash(const std::string& Error, const std::string& ExceptionType)
{
	// Log Size Limit Exceeded Throwing Exception
	if (!Error.starts_with("LSLETE"))
	{
		Log::Append(std::vformat(
			"CRASH - {}: {}",
			std::make_format_args(ExceptionType, Error)
		));
		Log::Save();
	}

	std::string errMessage = std::vformat(
		"An unexpected error occurred, and the application will now close. Details: \n\n{}\n\n{}",
		std::make_format_args(
			Error,
			"If this is the first time this has happened, please re-try. Otherwise, contact the developers."
		)
	);

	SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_ERROR,
		"Fatal Engine Error",
		errMessage.c_str(),
		nullptr
	);
}

int main(int argc, char** argv)
{
	Log::Info("Application startup...");

	Log::Info(std::format("Phoenix Engine, Main.cpp last compiled: {}", __DATE__));

	SDL_Window* window = nullptr;

	try
	{
		EngineObject engine{};

		try
		{
			engine.Initialize();
			EngineInstance = &engine;
			window = engine.Window;

			//Engine->MSAASamples = 2;

			Application(argc, argv);

			Log::Save(); // in case FileRW::WriteFile throws an exception
		}
		PHX_MAIN_CRASHHANDLERS;
	} // 25/12/2024 in case Engine's Destructor throws an exception
	PHX_MAIN_CRASHHANDLERS;

	// this occurs AFTER engine destructor is called
	// 15/12/2024
	Log::Info("Application shutting down...");

	Log::Save();
}
