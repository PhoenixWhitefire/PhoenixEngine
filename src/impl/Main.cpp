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

#include <filesystem>
#include <csignal>
#include <chrono>

#ifdef __GNUG__
#include <sys/wait.h>
#include <fcntl.h>
#endif

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
	std::vector<ObjectHandle> roots;

	if (!ScriptTool)
		roots = SceneFormat::Deserialize(FileRW::ReadFile(mapFile), &worldLoadSuccess);
	else
	{
		ObjectHandle dm = GameObjectManager::s_Create(EntityComponent::DataModel);
		ObjectHandle wp = GameObjectManager::s_Create(EntityComponent::Workspace);
		ObjectHandle cam = GameObjectManager::s_Create(EntityComponent::Camera);
		ObjectHandle light = GameObjectManager::s_Create(EntityComponent::DirectionalLight);

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

	const ObjectHandle& root = roots[0];
	PHX_ENSURE_MSG(root->FindComponent<EcDataModel>(), "Root Object was not a DataModel!");

	engine->BindDataModel(root);
	engine->PrimaryDataModel = root;
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

#define CRASHED_DIR "crash/"
#define CRASHED_APP_LOG CRASHED_DIR "application.txt"
#define CRASHED_HANDLER_LOG CRASHED_DIR "handler.txt"
#define CRASHED_APP_TRACE CRASHED_DIR "trace.txt"

#define APP_TRACE "application-crash-trace.txt"
#define APP_LOG "log.txt"
#define HANDLER_LOG "crash-handler-tmp.txt"

#define APPLICATION_ARG "--application"

static void processCliArgs(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		const char* v = argv[i];

		if (isBoolArgument(v, "--dev"))
		{
			EngineJsonConfig["Developer"] = checkBoolArgument(v, "--dev", true);
		}
		else if (strcmp(v, "--threads") == 0)
		{
			if (i + 1 < argc)
			{
				EngineJsonConfig["ThreadManagerThreadCount"] = std::stoi(argv[i + 1]);
				i++;
			}
			else
				Log.Error("'--threads' argument from command-line was not followed by the desired Thread Count");
		}
		else if (strcmp(v, "--tracy") == 0)
		{
			DeveloperTools::LaunchTracy();
		}
		else if (strcmp(v, "--apidump") == 0)
		{
			DoApiDump = true;
		}
		else if (strcmp(v, "--loadmap") == 0)
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
				Log.Error("'--loadmap' argument from command-line was not followed by the desired File");
		}
		else if (strcmp(v, "--tool") == 0)
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
				Log.Error("'--tool' argument from command-line was not followed by the desired File");
		}
		else if (isBoolArgument(v, "--headless"))
		{
			EngineJsonConfig["Headless"] = checkBoolArgument(v, "--headless", true);
		}
		else if (strcmp(v, "--x11") == 0)
		{
			glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
		}
		else if (strcmp(v, APPLICATION_ARG) == 0)
		{
			// Handler in `main`
		}
		else
		{
			Log.InfoF("Argument '{}' was not processed by Engine", v);
		}
	}
}

#if PHX_HEADLESS_BUILD
#define IS_HEADLESS_STR "Yes"
#else
#define IS_HEADLESS_STR "No"
#endif

#ifdef __GNUG__

extern "C" void handleCrashSignal(int signal);
extern "C" void handleCrashSignal(int sig)
{
	// create a basic trace back in case the system won't generate a coredump
	void* frames[64];
	int n = backtrace(frames, 64);

	int traceDescriptor = open(APP_TRACE, O_CREAT | O_WRONLY | O_TRUNC, 0644);

	if (traceDescriptor >= 0)
	{
		backtrace_symbols_fd(frames, n, traceDescriptor);
		close(traceDescriptor);
	}

	signal(sig, SIG_DFL);
	raise(sig); // re-raise so normal coredump is still generated if possible
}

#endif

static void installCrashSignalHandlers()
{
#ifdef __GNUG__

	for (int sig : { SIGSEGV, SIGABRT, SIGILL, SIGBUS })
		signal(sig, handleCrashSignal);

#endif
}

static void unsetQuitSignalHandlers()
{
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
}

extern "C" void handleQuitSignal(int signal);
extern "C" void handleQuitSignal(int signal)
{
    unsetQuitSignalHandlers();

    Engine* engine = Engine::Get();
    engine->SystemSignal = signal;
    engine->Close();
}

static void application(int argc, char** argv)
{
	installCrashSignalHandlers();
	processCliArgs(argc, argv);

    {
        Engine engine;
        Logging::IsGameObjectManagerAlive = true;

        if (DoApiDump)
            doApiDump();

        init();
        engine.argc = argc;
        engine.argv = argv;

        std::signal(SIGINT, handleQuitSignal);
        std::signal(SIGTERM, handleQuitSignal);

        engine.Start();

        unsetQuitSignalHandlers();
        Logging::Save();

        if (engine.SystemSignal != -1)
            Log.InfoF("Engine received signal {}", engine.SystemSignal);

        s_ExitCode = engine.ExitCode;
        DeveloperTools::Shutdown();
        engine.Shutdown();
    }

    Logging::IsGameObjectManagerAlive = false;
    Log.InfoF("The exit code is {}", s_ExitCode);
}

static void crashHandler(int argc, char** argv)
{
#ifdef __GNUG__
	pid_t pid = fork();
	if (pid < 0)
	{
		tinyfd_messageBox("Failed to launch", "fork failed", "ok", "error", 1);
		Log.Error("`fork` failed: {}", strerror(errno));
		s_ExitCode = 1;

		return;
	}

	if (pid == 0)
	{
		std::vector<char*> newArguments;
		newArguments.push_back(const_cast<char*>(argv[0]));
		newArguments.push_back(const_cast<char*>(APPLICATION_ARG));

		for (int i = 1; i < argc; i++)
			newArguments.push_back(argv[i]);

		newArguments.push_back(nullptr);

		Log.Info("Launching application\n\n");
		execv("/proc/self/exe", newArguments.data());

		tinyfd_messageBox("Failed to launch", "execv failed", "ok", "error", 1);
		Log.Error("`execv` failed: {}", strerror(errno));
		s_ExitCode = 1;

		return;
	}

	int status = 0;
	waitpid(pid, &status, 0);
	Log.Append("\n\n");

	if (WIFSIGNALED(status))
	{
		s_ExitCode = 1;

		int signal = WTERMSIG(status);
		tinyfd_messageBox(
			"Oops",
			"The game has crashed. Consider sending the core dump and logs :)\n\n"
				"Log files will be recorded to the " CRASHED_DIR " directory. If it already exists, it will be overwritten.",
			"ok",
			"error",
			1
		);
		Log.ErrorF("Application crashed: {}", strsignal(signal));
		Logging::Save();

		if (std::filesystem::is_directory(CRASHED_DIR))
		{
			std::error_code ec;
			std::filesystem::remove_all(CRASHED_DIR, ec);

			if (ec)
			{
				std::string error = std::format("Error removing the pre-existing " CRASHED_DIR " directory: {}", ec.message());
				tinyfd_messageBox("Error in crash handler", error.c_str(), "ok", "warning", 1);
				Log.Error(error);

				return;
			}
		}

		if (!std::filesystem::is_directory(CRASHED_DIR))
		{
			std::error_code mkdirEc;
			std::filesystem::create_directory(CRASHED_DIR, mkdirEc);

			if (mkdirEc)
			{
				std::string error = std::format("Failed to create " CRASHED_DIR " directory: {}", mkdirEc.message());
				tinyfd_messageBox("Error in crash handler", error.c_str(), "ok", "error", 1);
				Log.Error(error);

				return;
			}
		}

		std::error_code appLogCopyEc;
		std::filesystem::copy(APP_LOG, CRASHED_APP_LOG, appLogCopyEc);

		if (appLogCopyEc)
		{
			std::string error = std::format("Failed to copy application log file: {}", appLogCopyEc.message());
			tinyfd_messageBox("Error in crash handler", error.c_str(), "ok", "error", 1);
			Log.Error(error);

			return;
		}

		Logging::Save();

		std::error_code handlerLogCopyEc;
		std::filesystem::copy(HANDLER_LOG, CRASHED_HANDLER_LOG, handlerLogCopyEc);

		if (handlerLogCopyEc)
		{
			std::string error = std::format("Failed to copy crash handler log file: {}", handlerLogCopyEc.message());
			tinyfd_messageBox("Error in crash handler", error.c_str(), "ok", "error", 1);
			Log.Error(error);

			return;
		}

		Logging::LogFile = "./" CRASHED_HANDLER_LOG;
		Logging::Save(); // re-open handle

		if (std::filesystem::is_regular_file(APP_TRACE))
		{
			std::error_code ec;
			std::filesystem::rename(APP_TRACE, CRASHED_APP_TRACE, ec);

			if (ec)
			{
				std::string error = std::format("Failed to move backtrace: {}", ec.message());
				tinyfd_messageBox("Error in crash handler", error.c_str(), "ok", "error", 1);
				Log.Error(error);

				return;
			}
		}

		Log.Info("Created crash files successfully");
	}
#else
	(void)argc;
	(void)argv;
#endif
}

int main(int argc, char** argv)
{
	bool isCrashHandler = true;

#ifdef __GNUG__

	for (int i = 0; i < argc; i++)
	{
		if (strcmp(argv[i], APPLICATION_ARG) == 0)
			isCrashHandler = false;
	}

#else

	isCrashHandler = false;

#endif

	Logging::LogFile = isCrashHandler ? "./" HANDLER_LOG : "./" APP_LOG;

    Logging::Initialize();
    Log.Info(isCrashHandler ? "Crash handler startup" : "Application startup");

    Log.InfoF("Phoenix Engine"
        "\n\tVersion: {}"
        "\n\tCommit: {}"
        "\n\tTag: {}"
        "\n\tTarget platform: " PHX_TARGET_PLATFORM
        "\n\tTarget compiler: " PHX_TARGET_COMPILER
        "\n\tBuild type: " PHX_BUILD_TYPE
        "\n\tBuild date: {} @ {}"
        "\n\tHeadless: " IS_HEADLESS_STR,
        GetEngineVersion(), GetEngineCommitHash(), GetEngineCommitTag(), GetEngineBuildDate(), GetEngineBuildTime()
    );

    Log.Info("Command line: &&");

    for (int i = 0; i < argc; i++)
        if (i < argc - 1)
            Log.AppendF(" {}&&", argv[i]);
        else
            Log.AppendF(" {}", argv[i]);

    auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());

    Log.InfoF("Now: {:%F %T %Z}", now);

	if (isCrashHandler)
		crashHandler(argc, argv);
	else
		application(argc, argv);

    Log.Info(isCrashHandler ? "Crash handler shutdown" : "Application shutdown");
    Logging::Save();

	if (isCrashHandler)
		std::filesystem::remove(HANDLER_LOG);

    return s_ExitCode;
}
