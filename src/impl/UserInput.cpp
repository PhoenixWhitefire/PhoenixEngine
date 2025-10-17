#include "UserInput.hpp"

bool UserInput::IsKeyDown(int Key)
{
	return glfwGetKey(glfwGetCurrentContext(), Key) == GLFW_PRESS;
}

bool UserInput::IsMouseButtonDown(int Button)
{
	return glfwGetMouseButton(glfwGetCurrentContext(), Button) == GLFW_PRESS;
}
