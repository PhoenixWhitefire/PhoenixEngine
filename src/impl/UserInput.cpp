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

static bool isAnyMouseButtonDown()
{
    return UserInput::IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT) || UserInput::IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
}

bool UserInput::ShouldIgnoreUIInputSinking()
{
    if (!GImGui)
        return false;

    if (ImGui::IsAnyItemActive() && std::string_view(GImGui->ActiveIdWindow->Name).find("WindowOverViewport") == std::string::npos)
        return false;

    if (isAnyMouseButtonDown() && GImGui->HoveredWindow && std::string_view(GImGui->HoveredWindow->Name).find("WindowOverViewport") == std::string::npos)
        return false;

    return true;
}
