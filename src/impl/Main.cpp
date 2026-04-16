/*

Phoenix Engine

The oldest files I can find show that I started working on this on the 2nd of November, 2021.
Today, it is the 5th of March, 2023

This is a melting, sphaghetti code hodgepodge of random things I thought was cool when I was writing it
that SOMEHOW works without crashing atleast 50 times a frame.

Anyway, here is a small tour:

- "phoenix.conf" contains some configuration. Set "Developer" to "false" to disable the debug UIs.
- WASD to move horizontally, Q/E to move down/up. Left-click+drag to look around. LShift to move slower.
- F11 to toggle fullscreen.

https://github.com/Phoenixwhitefire/PhoenixEngine

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
#include <imgui/backends/imgui_impl_glfw.h>

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

#include <tracy/Tracy.hpp>

#include <tinyfiledialogs.h>

#include "Engine.hpp"

#include "asset/SceneFormat.hpp"
#include "component/Transform.hpp"
#include "component/Camera.hpp"
#include "script/ScriptEngine.hpp"

#include "GlobalJsonConfig.hpp"
#include "DeveloperTools.hpp"
#include "UserInput.hpp"
#include "Utilities.hpp"
#include "Version.hpp"
#include "Memory.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

static bool PreviouslyPressingF11 = false;
static bool WasRmbPressed = false;
static bool RmbTrigger = false;
static const float MouseSensitivity = 400.f;
static const float MovementSpeed = 15.f;
static double PrevMouseX, PrevMouseY = 0;

static ImGuiIO* GuiIO = nullptr;

static int s_ExitCode = 0;

static void handleInputs(double deltaTime)
{
	Engine* engine = Engine::Get();

	EcCamera* camera = engine->WorkspaceRef->FindComponent<EcWorkspace>()->GetSceneCamera()->FindComponent<EcCamera>();
	GLFWwindow* window = engine->Window;

	double mouseX;
	double mouseY;
	glfwGetCursorPos(window, &mouseX, &mouseY);
	
	if (camera->UseSimpleController && camera->Object->FindComponent<EcTransform>())
	{
		bool rmbPressed = (!GuiIO->WantCaptureMouse || UserInput::ShouldIgnoreUIInputSinking()) && UserInput::IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT);

		glm::mat4 camTrans = camera->GetWorldTransform();

		static const glm::vec3 WorldUp{ 0.f, 1.f, 0.f };
		glm::vec3 camForward = glm::vec3(camTrans[2]);
		glm::vec3 camUp = glm::vec3(camTrans[1]);

		if (!GuiIO->WantCaptureKeyboard || UserInput::ShouldIgnoreUIInputSinking())
		{
			float speed = MovementSpeed * static_cast<float>(deltaTime);

			if (UserInput::IsKeyDown(GLFW_KEY_LEFT_SHIFT))
				speed *= 0.5f;

			glm::vec3 position = (glm::vec3)camTrans[3];

			if (UserInput::IsKeyDown(GLFW_KEY_W))
				position += camForward * speed;

			if (UserInput::IsKeyDown(GLFW_KEY_A))
				position += -glm::normalize(glm::cross(camForward, WorldUp)) * speed;

			if (UserInput::IsKeyDown(GLFW_KEY_S))
				position += camForward * -speed;

			if (UserInput::IsKeyDown(GLFW_KEY_D))
				position += glm::normalize(glm::cross(camForward, WorldUp)) * speed;

			if (UserInput::IsKeyDown(GLFW_KEY_Q))
				position += camUp * -speed;

			if (UserInput::IsKeyDown(GLFW_KEY_E))
				position += camUp * speed;

			camTrans[3] = glm::vec4(position, 1.f);
		}

		RmbTrigger = false;
		if (rmbPressed && !WasRmbPressed)
			RmbTrigger = true;

		if (rmbPressed && !WasRmbPressed)
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			GuiIO->ConfigFlags |= ImGuiConfigFlags_NoMouse;
		}
		else if (!rmbPressed && WasRmbPressed)
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			GuiIO->ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		}

		WasRmbPressed = rmbPressed;

		if (rmbPressed)
		{
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

			glm::vec3 forward = camForward;
			glm::vec3 right = glm::normalize(glm::cross(WorldUp, forward));
			glm::vec3 up = glm::cross(forward, right);

			camTrans[0] = glm::vec4(right, 0.f);
			camTrans[1] = glm::vec4(up, 0.f);
			camTrans[2] = glm::vec4(camForward, 0.f);
		}

		camera->SetWorldTransform(camTrans);
	}

	PrevMouseX = mouseX;
	PrevMouseY = mouseY;

	if (UserInput::IsKeyDown(GLFW_KEY_F11))
	{
		if (!PreviouslyPressingF11)
		{
			PreviouslyPressingF11 = true;
			engine->SetIsFullscreen(!engine->IsFullscreen);
		}
	}
	else
		PreviouslyPressingF11 = false;
}

static void doApiDump()
{
	ZoneScoped;

	Log.Info("Dumping API...");

	nlohmann::json apiDump;
	apiDump["GameObject"] = GameObject::DumpApiToJson();
	apiDump["ScriptEnv"] = ScriptEngine::DumpApiToJson();

	PHX_CHECK(FileRW::WriteFile("./apidump.json", apiDump.dump(2)));
	Log.Info("API dump finished");
}

#ifdef NDEBUG
static void handleCrash(const std::string_view& Error, const std::string_view& ExceptionType)
{
	s_ExitCode = 1;

	// Log Size Limit Exceeded Throwing Exception
	if (!Error.starts_with("LSLETE"))
	{
		Log.AppendF(
			"CRASH - {}: {}",
			ExceptionType, Error
		);
		Logging::Save();
	}

	std::string errMessage = std::format(
		"An unexpected error occurred, and the application will now close. Details: \n\n{}\n\n{}",
		Error,
		"If this is the first time this has happened, please re-try. Otherwise, contact the developers."
	);

	// can fail, write to stderr if so 18/01/2025
	tinyfd_messageBox(
		"Fatal Error",
		errMessage.c_str(),
		"ok",
		"error",
		1
	);
}
#endif

static const char* MapFileFromArgs = nullptr;
static const char* ScriptTool = nullptr;

static void init()
{
	ZoneScoped;

	Engine* engine = Engine::Get();

	if (!engine->IsHeadlessMode)
	{
		Log.InfoF(
			"Initializing Dear ImGui {}...",
			IMGUI_VERSION
		);
	
		if (!IMGUI_CHECKVERSION())
			RAISE_RT("Dear ImGui detected a version mismatch");
	
		ImGui::CreateContext();
		GuiIO = &ImGui::GetIO();
		ImGui::StyleColorsDark();
		GuiIO->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		GuiIO->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		GuiIO->ConfigDpiScaleFonts = true;
		GuiIO->ConfigDpiScaleViewports = true;

		float displayScale = 0.f;
		glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &displayScale, nullptr);

		ImGui::GetStyle().ScaleAllSizes(displayScale);
		ImGui::GetStyle().DisplayWindowPadding = ImVec2(19.f, 19.f);

		PHX_ENSURE_MSG(ImGui_ImplGlfw_InitForOpenGL(engine->Window, true), "Failed to initialize Dear ImGui for GLFW");
		PHX_ENSURE_MSG(ImGui_ImplOpenGL3_Init("#version 460"), "Failed to initialize Dear ImGui for OpenGL");

		if (!std::filesystem::is_regular_file("imgui.ini"))
		{
			std::string defaultLayoutFile = EngineJsonConfig.value("DearImGuiDefaultLayoutFile", "default-layout.ini");

			if (std::filesystem::is_regular_file(defaultLayoutFile))
				std::filesystem::copy_file(defaultLayoutFile, "imgui.ini");
		}
	
		Log.Info("Dear ImGui initialized");
	
		engine->OnFrameStart.Connect(handleInputs);
	
		if (EngineJsonConfig.value("Developer", false))
		{
			Log.Info("Developer-mode specific functionality");
			DeveloperTools::Initialize(&engine->RendererContext);
			engine->OnFrameRenderGui.Connect(&DeveloperTools::Frame);
		}
	}

	Log.Info("Loading Root Scene from file...");

	const std::string& mapFile = MapFileFromArgs ?
									MapFileFromArgs
									: EngineJsonConfig.value("RootScene", "scenes/root.world");
	
	bool worldLoadSuccess = true;
	std::vector<ObjectRef> roots;

	if (!ScriptTool)
		roots = SceneFormat::Deserialize(FileRW::ReadFile(mapFile), &worldLoadSuccess);
	else
	{
		ObjectRef dm = GameObjectManager::s_Create(EntityComponent::DataModel);
		ObjectRef wp = GameObjectManager::s_Create(EntityComponent::Workspace);
		ObjectRef cam = GameObjectManager::s_Create(EntityComponent::Camera);
		ObjectRef light = GameObjectManager::s_Create(EntityComponent::DirectionalLight);

		wp->SetParent(dm);
		cam->SetParent(wp);
		light->SetParent(wp);
		wp->FindComponent<EcWorkspace>()->SetSceneCamera(cam);
		cam->FindComponent<EcCamera>()->UseSimpleController = true;

		dm->FindComponent<EcDataModel>()->LiveScripts = ScriptTool;

		roots.push_back(dm);
	}
	
	/*
	std::vector<GameObject, Memory::Allocator<GameObject>> memalloctest;
	memalloctest.reserve(5000);
	memalloctest.shrink_to_fit();
	*/

	PHX_ENSURE_MSG(worldLoadSuccess, "World failed to load: " + SceneFormat::GetLastErrorString());

	if (roots.size() > 1)
		Log.Warning("More than 1 root object in the World, anything other than the first will be ignored");

	PHX_ENSURE_MSG(!roots.empty(), "No root objects in World!");

	ObjectRef root = roots[0];
	PHX_ENSURE_MSG(root->FindComponent<EcDataModel>(), "Root Object was not a DataModel!");

	engine->BindDataModel(root);
}

static bool isBoolArgument(const char* v, const char* param)
{
	return (strlen(param) == (strlen(v) - 2) || strlen(param) == strlen(v)) && memcmp(v, param, strlen(param)) == 0;
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
		Log.ErrorF("Malformed boolean argument '{}' (matching '{}')", v, arg);
		return defaultVal;
	}

	if (vlen < alen + 2)
	{
		Log.ErrorF("Missing Y/N after '{}' (matching '{}')", v, arg);
		return defaultVal;
	}

	if (v[alen + 1] == 'Y')
		return true;
	if (v[alen + 1] == 'N')
		return false;

	Log.ErrorF("Invalid option for boolean '{}' in '{}' (matching '{}'), expected Y/N", v[alen+1], v, arg);
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
				Log.Error("'-threads' argument from command-line was not followed by the desired Thread Count");
		}
		else if (strcmp(v, "-tracyim") == 0)
		{
			DeveloperTools::LaunchTracy();
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

				Log.InfoF(
					"Map to load specified from launch argument. Map was: {}",
					MapFileFromArgs
				);

				i++;
			}
			else
				Log.Error("'-loadmap' argument from command-line was not followed by the desired File");
		}
		else if (strcmp(v, "-tool") == 0)
		{
			if (i + 1 < argc)
			{
				ScriptTool = argv[i + 1];

				Log.InfoF(
					"Standalone tool: {}",
					ScriptTool
				);

				i++;
			}
			else
				Log.Error("'-tool' argument from command-line was not followed by the desired File");
		}
		else if (isBoolArgument(v, "-headless"))
		{
			EngineJsonConfig["Headless"] = checkBoolArgument(v, "-headless", true);
		}
		else if (strcmp(v, "-x11") == 0)
		{
			glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
		}
	}
}

#if PHX_HEADLESS_BUILD
#define IS_HEADLESS_STR "Yes"
#else
#define IS_HEADLESS_STR "No"
#endif

int main(int argc, char** argv)
{
	Logging::Initialize();
	Log.Info("Application startup");
	
	Log.InfoF("Phoenix Engine");
	Log.AppendF(
		"\tVersion: {}"
		"\n\tCommit: {}"
		"\n\tTarget platform: " PHX_TARGET_PLATFORM "\n\tTarget compiler: " PHX_TARGET_COMPILER
		"\n\tBuild type: " PHX_BUILD_TYPE "\n\tBuild date: {} @ {}"
		"\n\tHeadless: " IS_HEADLESS_STR,
		GetEngineVersion(), GetEngineCommitHash(), GetEngineBuildDate(), GetEngineBuildTime()
	);

	Log.Info("Command line: &&");

	for (int i = 0; i < argc; i++)
		if (i < argc - 1)
			Log.AppendF(" {}&&", argv[i]);
		else
			Log.AppendF(" {}", argv[i]);

	processCliArgs(argc, argv);

	try
	{
		Engine engine;
		Log.Info("GameObjectManagerReady");

		if (DoApiDump)
			doApiDump();

		init();
		engine.argc = argc;
		engine.argv = argv;

		engine.Start();

		Logging::Save(); // in case FileRW::WriteFile throws an exception
		
		s_ExitCode = engine.ExitCode;
		DeveloperTools::Shutdown();
		engine.Shutdown();
	}
	PHX_MAIN_CRASHHANDLERS;

	Log.Info("GameObjectManagerDead");
	Log.InfoF("The exit code is {}", s_ExitCode);
	Log.Info("Application shutdown");
	Logging::Save();

	return s_ExitCode;
}
