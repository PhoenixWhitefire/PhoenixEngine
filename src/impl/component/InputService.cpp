#include <imgui.h>

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

#include <imgui_internal.h>

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

#include "component/InputService.hpp"
#include "UserInput.hpp"
#include "Utilities.hpp"
#include "Engine.hpp"

static GLFWcursor* DevCursor = nullptr;
static int DevCursorId = GLFW_ARROW_CURSOR;

const Reflection::StaticPropertyMap& PlayerInputComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY(
            "CursorMode",
            Integer,
            [](void*) -> Reflection::GenericValue
            {
                return { glfwGetInputMode(glfwGetCurrentContext(), GLFW_CURSOR) };
            },
            [](void*, const Reflection::GenericValue& gv)
            {
                int newMode = (int)gv.AsInteger();
                if (newMode == GLFW_CURSOR_DISABLED)
                    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
                else
                    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

                glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, newMode);
            }
        ),

        REFLECTION_PROPERTY(
            "IsKeyboardSunk",
            Boolean,
            [](void*) -> Reflection::GenericValue
            {
                return { UserInput::ShouldIgnoreUIInputSinking() ? false : ImGui::GetIO().WantCaptureKeyboard };
            },
            nullptr
        ),

        REFLECTION_PROPERTY(
            "IsMouseSunk",
            Boolean,
            [](void*) -> Reflection::GenericValue
            {
                return { UserInput::ShouldIgnoreUIInputSinking() ? false : ImGui::GetIO().WantCaptureMouse };
            },
            nullptr
        ),

        REFLECTION_PROPERTY(
            "Cursor",
            Integer,
            [](void*) -> Reflection::GenericValue
            {
                return DevCursorId;
            },
            [](void*, const Reflection::GenericValue& gv)
            {
                int cursor = (int)gv.AsInteger();
                if (cursor < GLFW_ARROW_CURSOR || cursor > GLFW_NOT_ALLOWED_CURSOR)
                    RAISE_RT("Invalid cursor '{}'", cursor);

                GLFWwindow* context = glfwGetCurrentContext();
                if (!context)
                    return;

                if (DevCursor)
                    glfwDestroyCursor(DevCursor);

                DevCursor = glfwCreateStandardCursor(cursor);
                glfwSetCursor(context, DevCursor);
                DevCursorId = cursor;
                glfwPollEvents(); // update cursor for the OS if we decide to do some heavy work right after this that doesn't poll events
            }
        )
    };

    return props;
}

const Reflection::StaticMethodMap& PlayerInputComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap methods = {
        { "IsKeyPressed", Reflection::MethodDescriptor{
            { Reflection::ValueType::Integer },
            { Reflection::ValueType::Boolean },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                return { UserInput::IsKeyDown((int)inputs[0].AsInteger()) };
            }
        } },

        { "IsMouseButtonPressed", Reflection::MethodDescriptor{
            { Reflection::ValueType::Integer },
            { Reflection::ValueType::Boolean },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                return { UserInput::IsMouseButtonDown((int)inputs[0].AsInteger()) };
            }
        } },

        { "GetCursorPosition", Reflection::MethodDescriptor{
            {},
            { Reflection::ValueType::Vector2 },
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                double x = 0;
                double y = 0;
                glfwGetCursorPos(glfwGetCurrentContext(), &x, &y);

                return { glm::vec2(x, y) };
            }
        } },

        { "SetViewportInputRect", Reflection::MethodDescriptor{
            { Reflection::ValueType::Vector2, Reflection::ValueType::Vector2 },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                Engine* engine = Engine::Get();
                glm::vec2 viewportPosition = inputs[0].AsVector2();
                glm::vec2 viewportSize = inputs[1].AsVector2();

                engine->ViewportInputPosition = { viewportPosition.x, viewportPosition.y };
                engine->OverrideViewportInputSize = { viewportSize.x, viewportSize.y };
                engine->OverrideDefaultViewportInputRect = true;

                return {};
            }
        } },

        { "ResetViewportInputRect", Reflection::MethodDescriptor{
            {},
            {},
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                Engine* engine = Engine::Get();
                engine->OverrideDefaultViewportInputRect = false;
                engine->ViewportInputPosition = {};

                return {};
            }
        } },
    };

    return methods;
}

// TODO: report bug
#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

const Reflection::StaticEventMap& PlayerInputComponentManager::GetEvents()
{
    static const Reflection::StaticEventMap events = {
        REFLECTION_EVENT(EcPlayerInput, KeyEvent, Reflection::ValueType::InputEvent),
        REFLECTION_EVENT(EcPlayerInput, MouseButtonEvent, Reflection::ValueType::InputEvent),
        REFLECTION_EVENT(EcPlayerInput, ScrollEvent, Reflection::ValueType::InputEvent)
    };

    return events;
}

#ifdef __GNUG__
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#endif

void PlayerInputComponentManager::Shutdown()
{
    if (DevCursor)
    {
        glfwDestroyCursor(DevCursor);
        DevCursor = nullptr;
    }
}
