#include <SDL3/SDL_mouse.h>

#include "UserInput.hpp"

const bool* KeyboardState = SDL_GetKeyboardState(NULL);

bool UserInput::IsKeyDown(SDL_Keycode Key)
{
	return KeyboardState[SDL_GetScancodeFromKey(Key, SDL_KMOD_NONE)] == 1;
}

bool UserInput::IsMouseButtonDown(bool Left)
{
	float x, y;
	uint32_t buttons;

	buttons = SDL_GetMouseState(&x, &y);

	if ((buttons & (Left ? SDL_BUTTON_LMASK : SDL_BUTTON_RMASK)) != 0)
		return true;

	return false;
}
