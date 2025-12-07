#include <imgui.h>

#include "component/InputService.hpp"
#include "UserInput.hpp"

class InputServiceManager : public ComponentManager<EcInputService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
            EC_PROP(
                "CursorMode",
                Integer,
                [](void* p) -> Reflection::GenericValue
                {
                    return { glfwGetInputMode(glfwGetCurrentContext(), GLFW_CURSOR) };
                },
                [](void* p, const Reflection::GenericValue& gv)
                {
                    glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, (int)gv.AsInteger());
                }
            ),

            EC_PROP(
                "IsKeyboardSunk",
                Boolean,
                [](void* p) -> Reflection::GenericValue
                {
                    return { ImGui::GetIO().WantCaptureKeyboard };
                },
                nullptr
            ),

            EC_PROP(
                "IsMouseSunk",
                Boolean,
                [](void* p) -> Reflection::GenericValue
                {
                    return { ImGui::GetIO().WantCaptureMouse };
                },
                nullptr
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
                [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    return { UserInput::IsKeyDown((int)inputs[0].AsInteger()) };
                }
            } },

            { "IsMouseButtonPressed", Reflection::MethodDescriptor{
                { Reflection::ValueType::Integer },
                { Reflection::ValueType::Boolean },
                [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    return { UserInput::IsMouseButtonDown((int)inputs[0].AsInteger()) };
                }
            } },

            { "GetCursorPosition", Reflection::MethodDescriptor{
                {},
                { Reflection::ValueType::Vector2 },
                [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
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
};

static InputServiceManager Instance;
