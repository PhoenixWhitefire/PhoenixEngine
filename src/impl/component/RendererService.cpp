// Renderer service, 29/01/2026
#include <glad/gl.h>

#include "Engine.hpp"
#include "component/RendererService.hpp"

class RendererServiceManager : public ComponentManager<EcRendererService>
{
    virtual const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
            REFLECTION_PROPERTY(
                "Resolution",
                Vector2,
                [](void*) -> Reflection::GenericValue
                {
                    Engine* engine = Engine::Get();
                    return glm::vec2(engine->WindowSizeX, engine->WindowSizeY);
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Engine* engine = Engine::Get();
                    const glm::vec2& size = gv.AsVector2();

                    engine->ResizeWindow(size.x, size.y);
                }
            ),
            REFLECTION_PROPERTY(
                "IsFullscreen",
                Boolean,
                [](void*) -> Reflection::GenericValue
                {
                    Engine* engine = Engine::Get();
                    return engine->IsFullscreen;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Engine* engine = Engine::Get();
                    engine->SetIsFullscreen(gv.AsBoolean());
                }
            ),
            REFLECTION_PROPERTY(
                "VSync",
                Boolean,
                [](void*) -> Reflection::GenericValue
                {
                    Engine* engine = Engine::Get();
                    return engine->VSync;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Engine* engine = Engine::Get();
                    engine->VSync = gv.AsBoolean();
                }
            ),
            REFLECTION_PROPERTY(
                "DebugWireframeRendering",
                Boolean,
                [](void*) -> Reflection::GenericValue
                {
                    Engine* engine = Engine::Get();
                    return engine->DebugWireframeRendering;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Engine* engine = Engine::Get();
                    engine->DebugWireframeRendering = gv.AsBoolean();
                }
            ),
        };

        return props;
    }

    virtual const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap methods = {
            { "UnloadTexture", Reflection::MethodDescriptor{
                { Reflection::ValueType::String },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    TextureManager* texManager = TextureManager::Get();
                    uint32_t resId = texManager->LoadTextureFromPath(std::string(inputs[0].AsStringView()), false);
                    texManager->UnloadTexture(resId);

                    return {};
                }
            } },

            { "SetShaderVariable", Reflection::MethodDescriptor{
                // Name of shader program, name of uniform, value, optional type of value (`Vector2` and `Vector3` are just `vector` in scripts)
                { Reflection::ValueType::String, Reflection::ValueType::String, Reflection::ValueType::Any, REFLECTION_OPTIONAL(Integer) },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    ShaderManager* shaderManager = ShaderManager::Get();
                    ShaderProgram& shader = shaderManager->GetShaderResource(shaderManager->LoadFromPath(inputs[0].AsString()));

                    Reflection::GenericValue val = inputs[2];
                    Reflection::ValueType vt = inputs.size() >= 4 ? (Reflection::ValueType)inputs[3].AsInteger() : val.Type;

                    if (val.Type == Reflection::ValueType::Vector3)
                    {
                        if (inputs.size() < 4)
                            RAISE_RT("For `vector` shader variables, the type must be explicitly provided (`Enum.ValueType.Vector2/Vector3`)");

                        if (vt == Reflection::ValueType::Vector2)
                            val = glm::vec2(val.AsVector3());
                    }

                    if (vt != val.Type)
                        RAISE_RTF("Mismatch between type of value ({}) and Type argument ({})", Reflection::TypeAsString(val.Type), Reflection::TypeAsString(vt));

                    using Reflection::ValueType;
                    const Reflection::ValueType AllowedTypes[] = {
                        ValueType::Boolean,
                        ValueType::Integer,
                        ValueType::Double,
                        ValueType::Vector2,
                        ValueType::Vector3,
                        ValueType::Color,
                        ValueType::Matrix,
                    };

                    bool allowed = false;
                    for (Reflection::ValueType a : AllowedTypes)
                    {
                        if (a == vt)
                        {
                            allowed = true;
                            break;
                        }
                    }

                    if (!allowed)
                        RAISE_RTF("{}s are not an allowed shader variable type!", Reflection::TypeAsString(vt));

                    shader.DefaultUniforms[inputs[1].AsString()] = val;
                    return {};
                }
            } },

            /*
            { "SetShaderTextureVariable", Reflection::MethodDescriptor{
                // Name of shader program, name of uniform, value, path to texture
                { Reflection::ValueType::String, Reflection::ValueType::String, Reflection::ValueType::String },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    ShaderManager* shaderManager = ShaderManager::Get();
                    ShaderProgram& shader = shaderManager->GetShaderResource(shaderManager->LoadFromPath(inputs[0].AsString()));

                    shader.SetTextureUniform(inputs[1].AsString(), TextureManager::Get()->LoadTextureFromPath(inputs[2].AsString()));
                    return {};
                }
            } },*/

            // SetMaterialVariable
        };

        return methods;
    }
};

static RendererServiceManager Instance;
