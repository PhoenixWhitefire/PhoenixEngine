#pragma once

#include<filesystem>
#include<nljson.h>

#include"gameobject/GameObjects.hpp"

#include"render/ShaderProgram.hpp"
#include"render/Renderer.hpp"

#include"datatype/Vector2.hpp"

#include"PhysicsEngine.hpp"
#include"ModelLoader.hpp"
#include"MapLoader.hpp"

// TODO: cleanup structure of public vs private
class EngineObject
{
public:
	/*
	Creates an Engine object
	@param The size of the window (overriden by `DefaultWindowSize` in the config file)
	@return An engine object
	*/

	EngineObject(Vector2 WindowStartSize, SDL_Window** WindowPtr);

	/*
	Gets the engine object, creates one if it doesn't exist already
	@param The size of the window (overriden by `DefaultWindowSize` in the config file)
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
	void OnWindowResized(int NewSizeX, int NewSizeY);

	Event<std::tuple<EngineObject*, double, double>> OnFrameStart = Event<std::tuple<EngineObject*, double, double>>();
	Event<std::tuple<EngineObject*, double, double>> OnFrameRenderGui = Event<std::tuple<EngineObject*, double, double>>();
	Event<std::tuple<EngineObject*, double, double>> OnFrameEnd = Event<std::tuple<EngineObject*, double, double>>();

	bool IsFullscreen = false;

	int WindowSizeX;
	int WindowSizeY;

	Object_DataModel* DataModel;
	Object_Workspace* Workspace;

	ShaderProgram* PostProcessingShaders;

	Renderer* RendererContext;
	SDL_Window* Window;

	bool Exit = false;
	
	int FpsCap = 60;

	int FramesPerSecond = 0;
	double FrameTime = 0.0f;

	bool VSync = false;

	float RunningTime = 0.0f;

private:
	int m_DrawnFramesInSecond = -1;

	uint32_t m_SDLWindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

	static EngineObject* Singleton;
};
