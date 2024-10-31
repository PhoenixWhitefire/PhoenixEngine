#pragma once

#include <SDL2/SDL.h>

namespace UserInput
{
	bool IsKeyDown(SDL_KeyCode Key);
	bool IsMouseButtonDown(bool Left);

	extern bool InputBeingSunk;
};
