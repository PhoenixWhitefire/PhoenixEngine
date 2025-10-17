#pragma once

#include <GLFW/glfw3.h>

namespace UserInput
{
	// expects GLFW keys
	bool IsKeyDown(int Key);
	// expects GLFW mouse buttons
	bool IsMouseButtonDown(int Button);
};
