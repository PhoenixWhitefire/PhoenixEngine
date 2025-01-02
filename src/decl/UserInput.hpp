#pragma once

#include <SDL3/SDL_keyboard.h>

namespace UserInput
{
	bool IsKeyDown(SDL_Keycode Key);
	bool IsMouseButtonDown(bool Left);

	extern bool InputBeingSunk;
};
