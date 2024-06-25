#pragma once

#include<filesystem>

#include<UserInput.hpp>

#include<GlobalJsonConfig.hpp>

#include<gameobject/GameObjects.hpp>

//#include"render/GraphicsAbstractionLayer.hpp"
#include"render/Renderer.hpp"
#include"render/ShaderProgram.hpp"

#include"PhysicsEngine.hpp"
#include"ModelLoader.hpp"
#include"MapLoader.hpp"

class EngineObject
{
public:
	/*
	Creates an Engine object
	@param The size of the window (in integer form with Vector2Int)
	@return An engine object
	*/

	EngineObject(Vector2 WindowStartSize, SDL_Window** WindowPtr);

	/*
	Gets the engine object, creates one if it doesn't exist already
	@param The size of the window
	@return The pointer to the object
	*/
	static EngineObject* Get(Vector2 WindowStartSize, SDL_Window** WindowPtr);

	/*
	Gets the engine object, creates one if it doesn't exist already. Uses a default window size of 800 by 800
	@return The pointer to the object
	*/
	static EngineObject* Get();

	/*
	Terminates the current engine thread, and calls destructors on internally-used engine objects
	*/
	~EngineObject();

	/*
	Initializes main engine loop
	Engine* function parameter is to allow code to use engine functions

	@param (Optional) Function to run at start of frame - Params<Engine*>,<double>,<double>: Current tick, Time since last frame
	@param (Optional) Function to run at end of frame - Params<Engine*>,<double>,<double>: Frame start tick, Time taken for frame
	*/
	void Start();

	void SetIsFullscreen(bool IsFullscreen);
	void ResizeWindow(int NewSizeX, int NewSizeY);

	std::vector<std::shared_ptr<GameObject>>& LoadModelAsMeshesAsync(
		const char* ModelFilePath,
		Vector3 Size,
		Vector3 Position,
		bool AutoParent = true
	);

	Event<std::tuple<EngineObject*, double, double>> OnFrameStart = Event<std::tuple<EngineObject*, double, double>>();
	Event<std::tuple<EngineObject*, double, double>> OnFrameRenderGui = Event<std::tuple<EngineObject*, double, double>>();
	Event<std::tuple<EngineObject*, double, double>> OnFrameEnd = Event<std::tuple<EngineObject*, double, double>>();

	PhysicsSolver* Physics = new PhysicsSolver();

	uint8_t MSAASamples = 0;

	float GlobalTimeMultiplier = 1.0f;

	bool IsFullscreen = false;

	int WindowSizeX;
	int WindowSizeY;

	ShaderProgram* PostProcessingShaders;
	ShaderProgram* Shaders3D;

	Renderer* m_renderer;

	SDL_Window* Window;

	std::shared_ptr<Object_Workspace> Game;

	bool Exit = false;
	
	static EngineObject* Singleton;

	int FpsCap = 60;

	int FramesPerSecond = 0;
	double FrameTime = 0.0f;

	bool VSync = false;

	float RunningTime = 0.0f;

	nlohmann::json GameConfig;

private:
	std::vector<MeshData_t> m_compileMeshData(std::shared_ptr<GameObject> RootObject);
	std::vector<LightData_t> m_compileLightData(std::shared_ptr<GameObject> RootObject);

	std::vector<std::shared_ptr<Object_ParticleEmitter>> m_particleEmitters;

	int m_DrawnFramesInSecond = -1;

	Mesh* m_DrawCallBatch;

	float RectangleVertices[24] = {
		 1.0f, -1.0f,    1.0f, 0.0f,
		-1.0f, -1.0f,    0.0f, 0.0f,
		-1.0f,  1.0f,    0.0f, 1.0f,

		 1.0f,  1.0f,    1.0f, 1.0f,
		 1.0f, -1.0f,    1.0f, 0.0f,
		-1.0f,  1.0f,    0.0f, 1.0f
	};

	VAO* SkyboxVAO = nullptr;
	VBO* SkyboxVBO = nullptr;
	EBO* SkyboxEBO = nullptr;

	uint32_t SDLWindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

	int m_LightIndex = 0;
};
