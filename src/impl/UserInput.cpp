#include <imgui_internal.h>
#include <string>

#include "UserInput.hpp"

bool UserInput::IsKeyDown(int Key)
{
	return glfwGetKey(glfwGetCurrentContext(), Key) == GLFW_PRESS;
}

bool UserInput::IsMouseButtonDown(int Button)
{
	return glfwGetMouseButton(glfwGetCurrentContext(), Button) == GLFW_PRESS;
}

bool UserInput::ShouldIgnoreUIInputSinking()
{
    if (GImGui && GImGui->HoveredWindow)
        if (std::string(GImGui->HoveredWindow->Name).find("WindowOverViewport") != std::string::npos)
            return true;

    return false;
}
