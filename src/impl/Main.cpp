/*

Phoenix Engine

The oldest files I can find show that I started working on this on the 2nd of November, 2021.
Today, it is the 5th of March, 2023

This is a melting, sphaghetti code hodgepodge of random things I thought was cool when I was writing it
that SOMEHOW works without crashing atleast 50 times a frame.

Anyway, here is a small tour:

- "phoenix.conf" contains some configuration. Set "Developer" to "false" to disable the debug UIs.
- WASD to move horizontally, Q/E to move down/up. Left-click to look around. Right-click to stop. LShift to move slower.
- Press `L` to dump GameObject API.
- `K` to reload all shaders, `I` to reload configuration
- F11 to toggle fullscreen.

https://github.com/Phoenixwhitefire/PhoenixEngine

*/

/*
	
	14/11/2024

	This Main file is in the "Player" or "Application" layer in my head, so I don't really care
	that it's messy.

*/

/*
	
	07/01/2025

	When an exception is thrown, generally it is either always fatal, or fatal contextually.
	For example, Luau APIs may throw exceptions, which are caught by Luau exception handlers,
	terminating the Script thread without killing the Engine. APIs such as `GameObject.Parent`
	from the Luau side, i.e. `GameObject::SetParent` from the Engine side, are fatal only to the
	Engine, intentionally. `Log::Error` is not fatal.
	
*/

#define GLM_ENABLE_EXPERIMENTAL

// `return 1` to indicate exit failure
// technically we should never fail to exit gracefully though, we are
// just indicating a fatal error occurred that forced the engine to
// quit
#define PHX_MAIN_HANDLECRASH(c) catch (c Error) { handleCrash(Error, #c); return 1; }

#define PHX_MAIN_HANDLECRASH_WHAT(c) catch (c Error) { handleCrash(Error.what(), #c); return 1; }

#ifdef NDEBUG

#define PHX_MAIN_CRASHHANDLERS                                                   \
PHX_MAIN_HANDLECRASH(const std::string&)                                         \
PHX_MAIN_HANDLECRASH(std::string_view)                                           \
PHX_MAIN_HANDLECRASH(const char*)                                                \
catch (const std::bad_alloc& AllocError)                                         \
{                                                                                \
	handleCrash(                                                                 \
		"System may have run out of memory: " + std::string(AllocError.what()),  \
		"std::bad_alloc"                                                         \
	);                                                                           \
	return 1;                                                                    \
}                                                                                \
PHX_MAIN_HANDLECRASH_WHAT(const std::runtime_error&)                             \
PHX_MAIN_HANDLECRASH_WHAT(const std::exception&)                                 \
catch (...)                                                                      \
{                                                                                \
	handleCrash("Unknown error", "Unknown (really helpful I know)");             \
	return 1;                                                                    \
}                                                                                \

#else

// (when running with a debugger) take me straight to the exception location
#define PHX_MAIN_CRASHHANDLERS catch (void*) { assert(false); }

#endif

#include <filesystem>

#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui_internal.h>

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_platform.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_init.h>

#include <tracy/Tracy.hpp>

#include "Engine.hpp"

#include "asset/SceneFormat.hpp"
#include "component/Camera.hpp"
#include "component/Script.hpp"
#include "script/ScriptEngine.hpp"

#include "GlobalJsonConfig.hpp"
#include "InlayEditor.hpp"
#include "UserInput.hpp"
#include "Utilities.hpp"
#include "Timing.hpp"
#include "Memory.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

static bool PreviouslyPressingF11 = false;
static bool MouseCaptured = false;
static const float MouseSensitivity = 400.f;
static const float MovementSpeed = 15.f;
static float PrevMouseX, PrevMouseY = 0;

static ImGuiIO* GuiIO = nullptr;

static int s_ExitCode = 0;

#ifdef _WIN32

#define popen _popen
#define pclose _pclose

// 13/01/2025 windows and it's quirkyness
#define LAUNCH_TRACY_CMD "\"Vendor\\tracy\\profiler\\build\\Release\\tracy-profiler.exe\" -a 127.0.0.1"

#else

#define LAUNCH_TRACY_CMD "\"Vendor/tracy/profiler/build/tracy-profiler\" -a 127.0.0.1"

#endif

#ifdef TRACY_ENABLE

static bool WasTracyLaunched = false;

// 13/01/2025: https://stackoverflow.com/a/478960
static std::string exec(const char* cmd)
{
	std::array<char, 128> buffer{ 0 };
	std::string result;
	FILE* pipe = popen(cmd, "r");

	if (!pipe)
		throw std::runtime_error("popen() failed!");

	while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
		result += buffer.data();
	
	pclose(pipe);

	return result;
}

#endif

static void launchTracy()
{
#ifdef TRACY_ENABLE
	if (WasTracyLaunched)
		RAISE_RT("Tried to launch Tracy twice in one session");
	WasTracyLaunched = true;

	std::thread(
		[]()
		{
			exec(LAUNCH_TRACY_CMD);
		}
	).detach();

#else

	PHX_ENSURE(SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_INFORMATION,
		"Tracy Integration",
		"Instrumentation was disabled for this build. You need to use a build with the `TRACY_ENABLE` macro defintion.",
		Engine::Get() ? Engine::Get()->Window : nullptr
	));

#endif
}

static void handleInputs(double deltaTime)
{
	Engine* EngineInstance = Engine::Get();

	EcCamera* camera = EngineInstance->WorkspaceRef->GetComponent<EcWorkspace>()->GetSceneCamera()->GetComponent<EcCamera>();
	SDL_Window* window = EngineInstance->Window;

	float mouseX;
	float mouseY;

	uint32_t activeMouseButton = SDL_GetMouseState(&mouseX, &mouseY);
	
	if (camera->UseSimpleController)
	{
		static const glm::vec3 WorldUp{ 0.f, 1.f, 0.f };
		glm::vec3 camForward = glm::vec3(camera->Transform[2]);
		glm::vec3 camUp = glm::vec3(camera->Transform[1]);

		if (!GuiIO->WantCaptureKeyboard)
		{
			float speed = MovementSpeed * static_cast<float>(deltaTime);

			if (UserInput::IsKeyDown(SDLK_LSHIFT))
				speed *= 0.5f;

			glm::vec3 position = (glm::vec3)camera->Transform[3];

			if (UserInput::IsKeyDown(SDLK_W))
				position += camForward * speed;

			if (UserInput::IsKeyDown(SDLK_A))
				position += -glm::normalize(glm::cross(camForward, WorldUp)) * speed;

			if (UserInput::IsKeyDown(SDLK_S))
				position += camForward * -speed;

			if (UserInput::IsKeyDown(SDLK_D))
				position += glm::normalize(glm::cross(camForward, WorldUp)) * speed;

			if (UserInput::IsKeyDown(SDLK_Q))
				position += camUp * -speed;

			if (UserInput::IsKeyDown(SDLK_E))
				position += camUp * speed;

			camera->Transform[3] = glm::vec4(position, camera->Transform[3].w);
		}

		if (MouseCaptured)
		{
			// Doesn't hide unless we tell Dear ImGui to hide the cursor too
			// (Otherwise it flickers)
			// 22/08/2024
			PHX_SDL_CALL(SDL_HideCursor);
			PHX_ENSURE(SDL_SetWindowMouseGrab(window, true));
			ImGui::SetMouseCursor(ImGuiMouseCursor_None);

			int windowSizeX, windowSizeY;
			PHX_ENSURE(SDL_GetWindowSize(window, &windowSizeX, &windowSizeY));

			float deltaMouseX = mouseX - PrevMouseX;
			float deltaMouseY = mouseY - PrevMouseY;

			float rotationX = deltaMouseY / MouseSensitivity;
			float rotationY = deltaMouseX / MouseSensitivity;

			glm::vec3 newForward = glm::rotate(
				camForward,
				-rotationX,
				glm::normalize(glm::cross(camForward, WorldUp))
			);

			if (abs(glm::angle(newForward, WorldUp) - glm::radians(90.f)) <= glm::radians(85.f))
				camForward = newForward;

			camForward = glm::rotate(camForward, -rotationY, WorldUp);
			camera->Transform[2] = glm::vec4(camForward, camera->Transform[2].w);

			// Keep the mouse in the window.
			// Teleport it to the other side if it hits the edge.

			float newMouseX = mouseX, newMouseY = mouseY;

			if (mouseX <= 10)
				newMouseX = static_cast<float>(windowSizeX - 20);

			if (mouseX >= windowSizeX - 10)
				newMouseX = 20;

			if (mouseY <= 10)
				newMouseY = static_cast<float>(windowSizeY - 20);

			if (mouseY >= windowSizeY - 10)
				newMouseY = 20;

			if (newMouseX != mouseX || newMouseY != mouseY)
			{
				SDL_WarpMouseInWindow(nullptr, newMouseX, newMouseY);
				mouseX = newMouseX;
				mouseY = newMouseY;
			}

			if (activeMouseButton & SDL_BUTTON_RMASK)
			{
				MouseCaptured = false;
				SDL_WarpMouseInWindow(nullptr, windowSizeX / 2.f, windowSizeY / 2.f);
			}
		}
		else
		{
			PHX_SDL_CALL(SDL_ShowCursor);
			PHX_ENSURE(SDL_SetWindowMouseGrab(window, false));
			ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

			if (activeMouseButton & SDL_BUTTON_LMASK && !GuiIO->WantCaptureMouse)
				MouseCaptured = true;
		}
	}

	PrevMouseX = mouseX;
	PrevMouseY = mouseY;

	if (UserInput::IsKeyDown(SDLK_F11))
	{
		if (!PreviouslyPressingF11)
		{
			PreviouslyPressingF11 = true;
			EngineInstance->SetIsFullscreen(!EngineInstance->IsFullscreen);
		}
	}
	else
		PreviouslyPressingF11 = false;
}

static void saveStatsCsvCallback(void* UserData, const char* const* FileList, int /* Filter */)
{
	if (!FileList)
	{
		Log::Error("Error trying to save stats to CSV: " + std::string(SDL_GetError()));
		return;
	}

	if (!FileList[0])
	{
		Log::Info("User did not select a save file path for the stats CSV");
		return;
	}

	std::string csvContents = *(std::string*)UserData;
	PHX_CHECK(FileRW::WriteFile(FileList[0], csvContents));
}

static void doApiDump()
{
	ZoneScoped;

	Log::Info("Dumping API...");

	nlohmann::json apiDump;
	apiDump["GameObject"] = GameObject::DumpApiToJson();
	apiDump["ScriptEnv"] = ScriptEngine::DumpApiToJson();

	PHX_CHECK(FileRW::WriteFile("./apidump.json", apiDump.dump(2)));
	Log::Info("API dump finished");
}

static void drawDeveloperUI(double DeltaTime)
{
	ZoneScopedC(tracy::Color::DarkOliveGreen);

	Engine* EngineInstance = Engine::Get();

	if (UserInput::IsKeyDown(SDLK_I) && !GuiIO->WantCaptureKeyboard)
	{
		Log::Info("Reloading configuration...");

		EngineInstance->LoadConfiguration();
	}

	if (UserInput::IsKeyDown(SDLK_K) && !GuiIO->WantCaptureKeyboard)
	{
		Log::Info("Reloading shaders...");

		ShaderManager::Get()->ReloadAll();

		Log::Info("Shaders reloaded");
	}

	if (ImGui::Begin("Info"))
	{
		constexpr size_t GraphDatapoints = 512;

		static std::array<double[255], GraphDatapoints> TimersHist;
		static std::array<decltype(Memory::Counters), GraphDatapoints> HeapUsageHist;
		static std::array<decltype(Memory::Counters), GraphDatapoints> HeapActivityHist;

		const double* times = Timing::FinalFrameTimes;
		const std::array<size_t, (size_t)Memory::Category::__count>& memcounts = Memory::Counters;
		
		uint8_t numTimers = Timing::StaticMagicTimerThing::s_NumTimers;

		// 0 - EntireFrame, 1 - FrameSleep, 2 - FrameWork, ...
		assert(strcmp(Timing::TimerNames[2], "FrameWork") == 0);
		size_t frameTime = static_cast<size_t>(times[1] * 1000);

		static bool IsSamplingStats = false;
		static std::string SampledCsv;

		ImGui::Text("FPS: %d", EngineInstance->FramesPerSecond);
		ImGui::Text("Frame time: %zims", frameTime);
		ImGui::Text("Draw calls: %u", EngineInstance->RendererContext.AccumulatedDrawCallCount);
		
		if (IsSamplingStats)
			SampledCsv.append(std::format(
				"{},{},{},",
				EngineInstance->FramesPerSecond,
				frameTime,
				EngineInstance->RendererContext.AccumulatedDrawCallCount
			));

#ifdef TRACY_ENABLE
		// 13/01/2025 hi hihihi hihiihii
		if (!WasTracyLaunched && ImGui::Button("Open Profiler"))
			launchTracy();
#endif

		static bool WasRmbPressed = false;
		static bool AreGraphsPaused = false;

		ImGui::BeginChild("Graphs", ImVec2(), ImGuiChildFlags_Borders);
		ImGui::Text("Right-click to");
		ImGui::SameLine();

		ImGuiStyle style = ImGui::GetStyle();

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - style.FramePadding.y);

		if (AreGraphsPaused)
		{
			if (ImGui::Button("resume"))
				AreGraphsPaused = false;
		}
		else if (ImGui::Button("pause"))
				AreGraphsPaused = true;

		ImGui::SameLine();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - style.FramePadding.y);

		ImGui::Text("graphs");
		
		if (GuiIO->WantCaptureMouse && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_RMASK))
		{
			if (!WasRmbPressed)
				AreGraphsPaused = !AreGraphsPaused;
			
			WasRmbPressed = true;
		}
		else
			WasRmbPressed = false;

		if (!AreGraphsPaused)
		{
			for (size_t i = 0; i < GraphDatapoints - 1; i++)
			{
				memcpy(TimersHist[i], TimersHist[i+1], sizeof(double) * 255);
				HeapUsageHist[i] = HeapUsageHist[i+1];
				HeapActivityHist[i] = HeapActivityHist[i+1];
			}

			memcpy(TimersHist[GraphDatapoints - 1], times, sizeof(double) * 255);
			HeapUsageHist[GraphDatapoints - 1] = Memory::Counters;
			HeapActivityHist[GraphDatapoints - 1] = Memory::Activity;
		}

		ImGui::SeparatorText("Timers");
		ImGui::PushID("Timers");

		for (int i = 0; i < numTimers; i++)
		{
			bool open = ImGui::TreeNodeEx(
				(void*)(int64_t)i,
				ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth,
				"%s: %zims",
				Timing::TimerNames[i],
				static_cast<size_t>(times[i] * 1000)
			);

			if (open)
			{
				float usageValues[GraphDatapoints] = { 0 };

				for (size_t hi = 0; hi < GraphDatapoints; hi++)
					usageValues[hi] = (float)TimersHist[hi][i] * 1000.f;
				
				ImGui::PlotLines("##", usageValues, GraphDatapoints, 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 100));

				ImGui::TreePop(); // ocornut why do you do this to me
			}
		}

		if (IsSamplingStats)
			for (int i = 0; i < numTimers; i++)
				SampledCsv.append(std::to_string(times[i]) + ",");

		ImGui::PopID();
		ImGui::SeparatorText("Heap");
		ImGui::PushID("Heap");

		for (int i = 0; i < (int)Memory::Category::__count; i++)
		{
			bool open = ImGui::TreeNodeEx(
				(void*)(int64_t)i,
				ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth,
				"%s: %zi",
				Memory::CategoryNames[i],
				memcounts[i]
			);

			if (open)
			{
				float usageValues[GraphDatapoints] = { 0 };
				float activityValues[GraphDatapoints] = { 0 };

				for (size_t hi = 0; hi < GraphDatapoints; hi++)
				{
					usageValues[hi] = (float)HeapUsageHist[hi][i];
					activityValues[hi] = (float)HeapActivityHist[hi][i];
				}
				
				ImGui::PlotLines("Usage", usageValues, GraphDatapoints, 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 100));
				ImGui::PlotLines("Activity", activityValues, GraphDatapoints, 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 100));

				ImGui::TreePop(); // ocornut why do you do this to me
			}
		}

		if (IsSamplingStats)
			for (int i = 0; i < (int)Memory::Category::__count; i++)
				SampledCsv += std::to_string(memcounts[i]) + ",";

		ImGui::PopID();
		ImGui::EndChild();

		ImGui::SeparatorText("Sampling");

		static double NumSecondsSampled = 0.f;
		static size_t NumFramesSampled = 0;

		if (IsSamplingStats)
		{
			NumSecondsSampled += DeltaTime;
			NumFramesSampled++;

			ImGui::Text("Sampled %zi frames over %.1f seconds", NumFramesSampled, NumSecondsSampled);
			ImGui::Text("Total size: %zi bytes (%zi bytes memory)", SampledCsv.size(), SampledCsv.capacity());

			if (ImGui::Button("End sampling & save to CSV"))
			{
				SDL_ShowSaveFileDialog(
					saveStatsCsvCallback,
					&SampledCsv,
					EngineInstance->Window,
					NULL,
					0,
					NULL
				);

				IsSamplingStats = false;
			}

			SampledCsv += "\n";
		}
		else
			if (ImGui::Button("Begin sampling"))
			{
				IsSamplingStats = true;
				SampledCsv = "FPS,Frame Time,Draw Calls,";
				NumSecondsSampled = 0.f;
				NumFramesSampled = 0;

				for (int i = 0; i < numTimers; i++)
					SampledCsv += Timing::TimerNames[i] + std::string(",");
				for (int i = 0; i < (int)Memory::Category::__count; i++)
					SampledCsv += Memory::CategoryNames[i] + std::string(",");

				SampledCsv += "\n";
			}

	}
	ImGui::End();

	if (ImGui::Begin("Settings"))
	{
		ImGui::Checkbox("VSync", &EngineInstance->VSync);

		bool wasFullscreen = EngineInstance->IsFullscreen;

		ImGui::Checkbox("Fullscreen", &EngineInstance->IsFullscreen);

		if (EngineInstance->IsFullscreen != wasFullscreen)
			EngineInstance->SetIsFullscreen(EngineInstance->IsFullscreen);

		if (!EngineInstance->VSync)
			ImGui::InputInt("FPS limit", &EngineInstance->FpsCap, 1, 30);

		bool postFxEnabled = EngineJsonConfig.value("postfx_enabled", false);

		ImGui::Checkbox("Post-Processing", &postFxEnabled);

		EngineJsonConfig["postfx_enabled"] = postFxEnabled;

		if (postFxEnabled)
		{
			float gammaCorrection = EngineJsonConfig.value("postfx_gamma", 1.f);

			ImGui::InputFloat("Gamma", &gammaCorrection);

			EngineJsonConfig["postfx_gamma"] = gammaCorrection;

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
			PHX_CHECK(FileRW::WriteFile("phoenix.conf", EngineJsonConfig.dump(2)));
			Log::Info("The JSON Config overwrote the pre-existing 'phoenix.conf'.");
		}

		ImGui::Checkbox("Wireframe rendering", &EngineInstance->DebugWireframeRendering);
		ImGui::Checkbox("Debug Collision AABBs", &EngineInstance->DebugAabbs);
	}
	ImGui::End();

	InlayEditor::UpdateAndRender(DeltaTime);
}

#ifdef NDEBUG
static void handleCrash(const std::string_view& Error, const std::string_view& ExceptionType)
{
	s_ExitCode = 1;

	// Log Size Limit Exceeded Throwing Exception
	if (!Error.starts_with("LSLETE"))
	{
		Log::Append(std::format(
			"CRASH - {}: {}",
			ExceptionType, Error
		));
		Log::Save();
	}

	std::string errMessage = std::format(
		"An unexpected error occurred, and the application will now close. Details: \n\n{}\n\n{}",
		Error,
		"If this is the first time this has happened, please re-try. Otherwise, contact the developers."
	);

	// can fail, write to stderr if so 18/01/2025
	bool showSuccess = SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_ERROR,
		"Fatal Error",
		errMessage.c_str(),
		Engine::Get() ? Engine::Get()->Window : nullptr
	);

	if (!showSuccess)
		fprintf(
			stderr,
			"Failed to show message box: %s.\n\nI'm not sure how you've done this. Check `log.txt` for the original crash reason.",
			SDL_GetError()
		);
}
#endif

static const char* MapFileFromArgs = nullptr;
static const char* ScriptTool = nullptr;

static void init()
{
	ZoneScoped;

	Engine* EngineInstance = Engine::Get();

	if (!EngineInstance->IsHeadlessMode)
	{
		Log::InfoF(
			"Initializing Dear ImGui {}...",
			IMGUI_VERSION
		);
	
		if (!IMGUI_CHECKVERSION())
			RAISE_RT("Dear ImGui detected a version mismatch");
	
		ImGui::CreateContext();
	
		GuiIO = &ImGui::GetIO();
		ImGui::StyleColorsDark();
	
		PHX_ENSURE_MSG(ImGui_ImplSDL3_InitForOpenGL(
			EngineInstance->Window,
			EngineInstance->RendererContext.GLContext
		), "`ImGui_ImplSDL3_InitForOpenGL` failed");
		
		PHX_ENSURE_MSG(ImGui_ImplOpenGL3_Init("#version 460"), "`ImGui_ImplOpenGL3_Init` failed");
	
		Log::Info("Dear ImGui initialized");
	
		EngineInstance->OnFrameStart.Connect(handleInputs);
	
		if (EngineJsonConfig.value("Developer", false))
		{
			Log::Info("Developer-mode specific functionality");
			InlayEditor::Initialize(&EngineInstance->RendererContext);
			EngineInstance->OnFrameRenderGui.Connect(drawDeveloperUI);
		}
	}

	Log::Info("Loading Root Scene from file...");

	const std::string& mapFile = MapFileFromArgs ?
									MapFileFromArgs
									: EngineJsonConfig.value("RootScene", "scenes/root.world");
	
	bool worldLoadSuccess = true;
	std::vector<ObjectRef> roots;

	if (!ScriptTool)
		roots = SceneFormat::Deserialize(FileRW::ReadFile(mapFile), &worldLoadSuccess);
	else
	{
		ObjectRef dm = GameObject::Create(EntityComponent::DataModel);
		ObjectRef wp = GameObject::Create(EntityComponent::Workspace);
		ObjectRef cam = GameObject::Create(EntityComponent::Camera);

		wp->SetParent(dm);
		cam->SetParent(wp);
		wp->GetComponent<EcWorkspace>()->SetSceneCamera(cam);

		GameObject* script = GameObject::Create(EntityComponent::Script);
		script->GetComponent<EcScript>()->SourceFile = ScriptTool;
		script->Name = "Tool";
		script->SetParent(wp);

		roots.push_back(dm);
	}
	
	/*
	std::vector<GameObject, Memory::Allocator<GameObject>> memalloctest;
	memalloctest.reserve(5000);
	memalloctest.shrink_to_fit();
	*/

	PHX_ENSURE_MSG(worldLoadSuccess, "World failed to load: " + SceneFormat::GetLastErrorString());

	if (roots.size() > 1)
		Log::Warning("More than 1 root object in the World, anything other than the first will be ignored");

	PHX_ENSURE_MSG(!roots.empty(), "No root objects in World!");

	ObjectRef root = roots[0];
	PHX_ENSURE_MSG(root->GetComponent<EcDataModel>(), "Root Object was not a DataModel!");

	root->IncrementHardRefs();
	EngineInstance->BindDataModel(root);
}

static bool isBoolArgument(const char* v, const char* arg)
{
	return memcmp(v, arg, std::min(strlen(v), strlen(arg))) == 0;
}

static bool checkBoolArgument(const char* v, const char* arg, bool defaultVal)
{
	size_t alen = strlen(arg);
	size_t vlen = strlen(v);

	if (vlen == alen) // no `:`
		return defaultVal;

	assert(vlen > alen); // shouldve been caught by `isBoolArgument`

	if (v[alen] != ':')
	{
		Log::ErrorF("Malformed boolean argument '{}' (matching '{}')", v, arg);
		return defaultVal;
	}

	if (vlen < alen + 2)
	{
		Log::ErrorF("Missing Y/N after '{}' (matching '{}')", v, arg);
		return defaultVal;
	}

	if (v[alen + 1] == 'Y')
		return true;
	if (v[alen + 1] == 'N')
		return false;

	Log::ErrorF("Invalid option for boolean '{}' in '{}' (matching '{}'), expected Y/N", v[alen+1], v, arg);
	return defaultVal;
}

static bool DoApiDump = false;

static void processCliArgs(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		const char* v = argv[i];

		if (isBoolArgument(v, "-dev"))
		{
			EngineJsonConfig["Developer"] = checkBoolArgument(v, "-dev", true);
		}
		else if (strcmp(v, "-threads") == 0)
		{
			if (i + 1 < argc)
			{
				EngineJsonConfig["ThreadManagerThreadCount"] = std::stoi(argv[i + 1]);
				i++;
			}
			else
				Log::Error("'-threads' argument from command-line was not followed by the desired Thread Count");
		}
		else if (strcmp(v, "-tracyim") == 0)
		{
			launchTracy();

			// took ~220ms on my machine for tracy to launch, double it and give it to the next person
			// 13/01/2025
			std::this_thread::sleep_for(std::chrono::milliseconds(400));
		}
		else if (strcmp(v, "-apidump") == 0)
		{
			DoApiDump = true;
		}
		else if (strcmp(v, "-loadmap") == 0)
		{
			if (i + 1 < argc)
			{
				MapFileFromArgs = argv[i + 1];

				Log::InfoF(
					"Map to load specified from launch argument. Map was: {}",
					MapFileFromArgs
				);

				i++;
			}
			else
				Log::Error("'-loadmap' argument from command-line was not followed by the desired File");
		}
		else if (strcmp(v, "-tool") == 0)
		{
			if (i + 1 < argc)
			{
				ScriptTool = argv[i + 1];

				Log::InfoF(
					"Standalone tool: {}",
					ScriptTool
				);

				i++;
			}
			else
				Log::Error("'-script' argument from command-line was not followed by the desired File");
		}
		else if (isBoolArgument(v, "-headless"))
		{
			EngineJsonConfig["Headless"] = checkBoolArgument(v, "-headless", true);
		}
		else
			Log::ErrorF("Unknown CLI argument '{}'", v);
	}
}

int main(int argc, char** argv)
{
	Log::Info("Application startup");
	
	Log::InfoF(
		"Phoenix Engine:\n\tTarget platform: {}\n\tBuild type: {}\n\tMain.cpp last compiled: {} @ {}",
		SDL_GetPlatform(), PHX_BUILD_TYPE, __DATE__, __TIME__
	);

	Log::Info("Command line: &&");

	for (int i = 0; i < argc; i++)
		if (i < argc - 1)
			Log::Append(" " + std::string(argv[i]) + "&&");
		else
			Log::Append(" " + std::string(argv[i]));

	processCliArgs(argc, argv);

	try
	{
		Engine engine{};
		if (DoApiDump)
			doApiDump();

		init();

		engine.Start();

		Log::Save(); // in case FileRW::WriteFile throws an exception
		
		s_ExitCode = engine.ExitCode;
		InlayEditor::Shutdown();
		engine.Shutdown();
	}
	PHX_MAIN_CRASHHANDLERS;

	Log::Info("Application shutdown");

	Log::Save();

	return s_ExitCode;
}
