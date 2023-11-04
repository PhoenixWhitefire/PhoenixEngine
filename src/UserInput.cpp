#include<UserInput.hpp>

const uint8_t* KeyboardState = SDL_GetKeyboardState(NULL);

bool UserInput::IsKeyDown(SDL_KeyCode Key)
{
	return KeyboardState[SDL_GetScancodeFromKey(Key)] == 1;
}

bool UserInput::IsMouseButtonDown(unsigned int SDLMouseButtonMask)
{
	int x, y;
	Uint32 Buttons;

	Buttons = SDL_GetMouseState(&x, &y);

	if ((Buttons & SDLMouseButtonMask) != 0)
		return true;

	return false;
}
