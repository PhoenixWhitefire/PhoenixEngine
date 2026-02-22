#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

#include <imgui_internal.h>

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

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
