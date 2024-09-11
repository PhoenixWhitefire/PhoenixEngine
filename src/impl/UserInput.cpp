#include"UserInput.hpp"

const uint8_t* KeyboardState = SDL_GetKeyboardState(NULL);

bool UserInput::InputBeingSunk = false;

bool UserInput::IsKeyDown(SDL_KeyCode Key)
{
	return KeyboardState[SDL_GetScancodeFromKey(Key)] == 1;
}

bool UserInput::IsMouseButtonDown(uint32_t SDLMouseButtonMask)
{
	int x, y;
	uint32_t buttons;

	buttons = SDL_GetMouseState(&x, &y);

	if ((buttons & SDLMouseButtonMask) != 0)
		return true;

	return false;
}
