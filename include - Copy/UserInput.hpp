#pragma once

#include<SDL2/SDL.h>

class UserInput
{
public:
	static bool IsKeyDown(SDL_KeyCode Key);
	static bool IsMouseButtonDown(unsigned int SDLMouseButtonMask);
};
