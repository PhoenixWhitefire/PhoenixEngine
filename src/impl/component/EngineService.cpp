// Engine service GameObject component

#include <tinyfiledialogs.h>
#include <tracy/Tracy.hpp>

#include "component/EngineService.hpp"
#include "asset/TextureManager.hpp"
#include "script/ScriptEngine.hpp"
#include "GlobalJsonConfig.hpp"
#include "InlayEditor.hpp"
#include "Version.hpp"
#include "Engine.hpp"

static const std::string_view Tools[] =
{
    "Tool_Explorer",
    "Tool_Properties",
    "Tool_Materials",
    "Tool_Shaders",
    "Tool_Settings",
    "Tool_Info"
};

static std::string checkValidTool(std::string toolName)
{
    bool isValidTool = false;

    for (const std::string_view& tool : Tools)
    {
        if (tool == toolName)
        {
            isValidTool = true;
            break;
        }
    }

    if (!isValidTool)
        RAISE_RTF("Invalid tool '{}'", toolName);

    return toolName;
}

class EngineServiceManager : public ComponentManager<EcEngine>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
            REFLECTION_PROPERTY(
                "WindowSize",
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
                "IsHeadless",
                Boolean,
                [](void*) -> Reflection::GenericValue
                {
                    Engine* engine = Engine::Get();
                    return engine->IsHeadlessMode;
                },
                nullptr
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
                "Framerate",
                Integer,
                [](void*) -> Reflection::GenericValue
                {
                    Engine* engine = Engine::Get();
                    return engine->FramesPerSecond;
                },
                nullptr
            ),
            REFLECTION_PROPERTY(
                "FramerateCap",
                Integer,
                [](void*) -> Reflection::GenericValue
                {
                    Engine* engine = Engine::Get();
                    return engine->FpsCap;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Engine* engine = Engine::Get();
                    engine->FpsCap = gv.AsInteger();
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
            REFLECTION_PROPERTY(
                "DebugCollisionAabbs",
                Boolean,
                [](void*) -> Reflection::GenericValue
                {
                    Engine* engine = Engine::Get();
                    return engine->DebugAabbs;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Engine* engine = Engine::Get();
                    engine->DebugAabbs = gv.AsBoolean();
                }
            ),
            REFLECTION_PROPERTY(
                "DebugSpatialHeat",
                Boolean,
                [](void*) -> Reflection::GenericValue
                {
                    Engine* engine = Engine::Get();
                    return engine->DebugSpatialHeat;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Engine* engine = Engine::Get();
                    engine->DebugSpatialHeat = gv.AsBoolean();
                }
            ),
            REFLECTION_PROPERTY(
                "PhysicsTimescale",
                Double,
                [](void*) -> Reflection::GenericValue
                {
                    Engine* engine = Engine::Get();
                    return engine->PhysicsTimeScale;
                },
                [](void*, const Reflection::GenericValue& gv)
                {
                    Engine* engine = Engine::Get();
                    engine->PhysicsTimeScale = gv.AsDouble();
                }
            ),
            REFLECTION_PROPERTY(
                "Version",
                String,
                [](void*) -> Reflection::GenericValue
                {
                    return GetEngineVersion();
                },
                nullptr
            ),
            REFLECTION_PROPERTY(
                "CommitHash",
                String,
                [](void*) -> Reflection::GenericValue
                {
                    return GetEngineCommitHash();
                },
                nullptr
            ),
        };

        return props;
    }

    const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap methods = {
            { "Exit", Reflection::MethodDescriptor{
                { REFLECTION_OPTIONAL(Integer) },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    Engine* engine = Engine::Get();
                    engine->ExitCode = inputs.size() > 0 ? inputs[0].AsInteger() : 0;
                    engine->Close();

                    return {};
                }
            } },

            { "BindDataModel", Reflection::MethodDescriptor{
                { Reflection::ValueType::GameObject },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    Engine* engine = Engine::Get();
                    engine->BindDataModel(GameObject::FromGenericValue(inputs[0]));

                    return {};
                }
            } },

            { "SetExplorerRoot", Reflection::MethodDescriptor{
                { Reflection::ValueType::GameObject },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    InlayEditor::SetExplorerRoot(GameObject::FromGenericValue(inputs[0]));

                    return {};
                }
            } },

            { "SetExplorerSelections", Reflection::MethodDescriptor{
                { Reflection::ValueType::Array },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    const std::span<Reflection::GenericValue>& inner = inputs[0].AsArray();

                    std::vector<ObjectHandle> objects;
                    objects.reserve(inner.size());

                    for (const Reflection::GenericValue& gv : inner)
                        objects.emplace_back(GameObject::FromGenericValue(gv));

                    InlayEditor::SetExplorerSelections(objects);
                    return {};
                }
            } },

            { "GetExplorerSelections", Reflection::MethodDescriptor{
                {},
                { Reflection::ValueType::Array },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    const auto& sels = InlayEditor::GetExplorerSelections();

                    std::vector<Reflection::GenericValue> out;
                    out.reserve(sels.size());

                    for (const ObjectHandle& obj : sels)
                        out.push_back(obj->ToGenericValue());

                    return { out };
                }
            } },

            { "CreateVM", Reflection::MethodDescriptor{
                { Reflection::ValueType::String },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    ScriptEngine::RegisterNewVM(std::string(inputs[0].AsStringView()));

                    return {};
                }
            } },

            { "CloseVM", Reflection::MethodDescriptor{
                { Reflection::ValueType::String },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    const auto& it = ScriptEngine::VMs.find(std::string(inputs[0].AsStringView()));
                    if (it == ScriptEngine::VMs.end())
                        RAISE_RT("Invalid VM");

                    ScriptEngine::L::Close(it->second.MainThread);
                    ScriptEngine::VMs.erase(it);

                    return {};
                }
            } },

            { "GetCurrentVM", Reflection::MethodDescriptor{
                {},
                { Reflection::ValueType::String },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    return { ScriptEngine::CurrentVM };
                }
            } },

            { "SetCurrentVM", Reflection::MethodDescriptor{
                { Reflection::ValueType::String },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    const std::string vm = std::string(inputs[0].AsStringView());
                    if (ScriptEngine::VMs.find(vm) == ScriptEngine::VMs.end())
                        RAISE_RTF("Invalid VM {}", vm);

                    ScriptEngine::CurrentVM = vm;

                    return {};
                }
            } },

            { "RunInVM", Reflection::MethodDescriptor{
                { Reflection::ValueType::String, Reflection::ValueType::String, REFLECTION_OPTIONAL(String) },
                { Reflection::ValueType::Boolean, REFLECTION_OPTIONAL(String) },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    const std::string vmName = std::string(inputs[0].AsStringView());

                    const auto& it = ScriptEngine::VMs.find(vmName);
                    if (it == ScriptEngine::VMs.end())
                        RAISE_RTF("Invalid VM '{}'", vmName);

                    const ScriptEngine::LuauVM& vm = it->second;

                    const std::string code = std::string(inputs[1].AsStringView());
	                const std::string chname = inputs.size() > 2 ? std::string(inputs[2].AsStringView()) : code;

	                lua_State* ML = lua_newthread(vm.MainThread);

                    if (ScriptEngine::CompileAndLoad(ML, code, chname) == 0)
                    {
                        int result = lua_resume(ML, ML, 0);

                        if (result == LUA_ERRERR || result == LUA_ERRRUN || result == LUA_ERRMEM)
                            Log::Error(lua_tostring(ML, -1), std::format("SourceChunkName:{}", chname));

                        const char* const ResultToMessage[] =
                        {
                            "ok",
                            "yield",
                            nullptr,
                            nullptr,
                            nullptr,
                            nullptr,
                            "break"
                        };

                        const char* message = ResultToMessage[result] ? ResultToMessage[result] : lua_tostring(ML, -1);
                        return { result == LUA_OK, message };
	                }
	                else
                    {
                        Log::Error(lua_tostring(ML, -1), std::format("SourceChunkName:{}", chname));

                        return { false, lua_tostring(ML, -1) };
                    }
                }
            } },

            { "GetToolNames", Reflection::MethodDescriptor{
                {},
                { Reflection::ValueType::Array },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    std::vector<Reflection::GenericValue> out;
                    out.reserve(std::size(Tools));

                    for (const std::string_view& tool : Tools)
                        out.emplace_back(tool);

                    return { out };
                }
            } },

            { "SetToolEnabled", Reflection::MethodDescriptor{
                { Reflection::ValueType::String, Reflection::ValueType::Boolean },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    std::string requestedTool = checkValidTool(std::string(inputs[0].AsStringView()));
                    EngineJsonConfig[requestedTool] = inputs[1].AsBoolean();

                    return {};
                }
            } },

            { "IsToolEnabled", Reflection::MethodDescriptor{
                { Reflection::ValueType::String },
                { Reflection::ValueType::Boolean },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    return { (bool)EngineJsonConfig[checkValidTool(std::string(inputs[0].AsStringView()))] };
                }
            } },

            { "ShowMessageBox", Reflection::MethodDescriptor{
                { Reflection::ValueType::String, Reflection::ValueType::String, REFLECTION_OPTIONAL(String), REFLECTION_OPTIONAL(String), REFLECTION_OPTIONAL(Integer) },
                { Reflection::ValueType::Integer },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    return { tinyfd_messageBox(
                        inputs[0].AsStringView().data(),                              // title
                        inputs[1].AsStringView().data(),                              // message
                        inputs.size() > 2 ? inputs[2].AsStringView().data() : "ok",   // buttons
                        inputs.size() > 3 ? inputs[3].AsStringView().data() : "info", // icon
                        inputs.size() > 4 ? inputs[4].AsInteger() : 1                 // default button
                    ) };
                }
            } },

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

            { "GetCliArguments", Reflection::MethodDescriptor{
                {},
                { Reflection::ValueType::Array },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    Engine* engine = Engine::Get();

                    std::vector<Reflection::GenericValue> out;
                    out.reserve(engine->argc);

                    for (int i = 0; i < engine->argc; i++)
                        out.emplace_back(engine->argv[i]);

                    return { out };
                }
            } },

            { "SetViewport", Reflection::MethodDescriptor{
                { Reflection::ValueType::Vector2, Reflection::ValueType::Vector2 },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    Engine* engine = Engine::Get();
                    glm::vec2 viewportPosition = inputs[0].AsVector2();
                    glm::vec2 viewportSize = inputs[1].AsVector2();

                    engine->ViewportPosition = { viewportPosition.x, viewportPosition.y };
                    engine->OverrideViewportSize = { viewportSize.x, viewportSize.y };
                    engine->OverrideDefaultViewport = true;

                    return {};
                }
            } },

            { "ResetViewport", Reflection::MethodDescriptor{
                {},
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    Engine* engine = Engine::Get();
                    engine->OverrideDefaultViewport = false;
                    engine->ViewportPosition = {};

                    return {};
                }
            } },
        };

        return methods;
    }

    const Reflection::StaticEventMap& GetEvents() override
    {
        static const Reflection::StaticEventMap events = {
            REFLECTION_EVENT(EcEngine, OnMessageLogged, Reflection::ValueType::Integer, Reflection::ValueType::String, Reflection::ValueType::String)
        };

        return events;
    }
};

static EngineServiceManager Instance;

void EcEngine::SignalNewLogMessage(LogMessageType Type, const std::string_view& Message, const std::string_view& ExtraTags)
{
    for (const EcEngine& ce : Instance.m_Components)
    {
        if (ce.Valid)
            REFLECTION_SIGNAL_EVENT(ce.OnMessageLoggedCallbacks, (int)Type, Message, ExtraTags);
    }
}
