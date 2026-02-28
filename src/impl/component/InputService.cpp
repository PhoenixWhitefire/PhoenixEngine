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

static GLFWcursor* DevCursor = nullptr;
static int DevCursorId = GLFW_ARROW_CURSOR;

class InputServiceManager : public ComponentManager<EcPlayerInput>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
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
                    glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, (int)gv.AsInteger());
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
                        RAISE_RTF("Invalid cursor '{}'", cursor);

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

    const Reflection::StaticMethodMap& GetMethods() override
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
        };

        return methods;
    }

// TODO: report bug
#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

    const Reflection::StaticEventMap& GetEvents() override
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

    void Shutdown() override
    {
        if (DevCursor)
        {
            glfwDestroyCursor(DevCursor);
            DevCursor = nullptr;
        }
    }
};

static InputServiceManager Instance;
