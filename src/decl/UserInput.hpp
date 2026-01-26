#pragma once

#include <GLFW/glfw3.h>

namespace UserInput
{
	// expects GLFW keys
	bool IsKeyDown(int Key);
	// expects GLFW mouse buttons
	bool IsMouseButtonDown(int Button);

	// Dear ImGui is saying that it needs keyboard and mouse input when interacting with the Viewport window
	// of the Editor, which has the `NoInputs` flag set. Not sure if this is intentional behavior but it is
	// very, very annoying.
	bool ShouldIgnoreUIInputSinking();
};
