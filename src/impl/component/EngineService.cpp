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
#include "FileRW.hpp"

static const std::string_view Tools[] = {
    "Tool_Explorer",
    "Tool_Properties",
    "Tool_Materials",
    "Tool_Shaders",
    "Tool_Settings",
    "Tool_Info",
    "Tool_ScriptExplorer"
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

// Also in `ScriptEngine.cpp`!!
#define JSON_ENCODED_DATA_TAG "__HX_EncodedData"

static Reflection::GenericValue jsonToGeneric(const nlohmann::json& v)
{
    switch (v.type())
	{
	case nlohmann::json::value_t::null:
        return Reflection::GenericValue::Null();

	case nlohmann::json::value_t::boolean:
        return Reflection::GenericValue((bool)v);

	case nlohmann::json::value_t::number_integer:
        return Reflection::GenericValue((int)v);

	case nlohmann::json::value_t::number_unsigned:
        return Reflection::GenericValue((uint32_t)v);

	case nlohmann::json::value_t::number_float:
        return Reflection::GenericValue((float)v);

	case nlohmann::json::value_t::string:
		return Reflection::GenericValue((std::string)v);

	case nlohmann::json::value_t::array:
	{
        std::vector<Reflection::GenericValue> arr;
		for (size_t i = 0; i < v.size(); i++)
            arr.push_back(jsonToGeneric(v[i]));

		return arr;
	}
	case nlohmann::json::value_t::object:
	{
        std::vector<Reflection::GenericValue> arr;

		for (auto it = v.begin(); it != v.end(); ++it)
		{
			std::string key = it.key();
			const nlohmann::json& data = it.value();
            arr.emplace_back(key);

            Reflection::GenericValue val;

			if (key == JSON_ENCODED_DATA_TAG)
			{
				const std::string& type = data["type"];
				const nlohmann::json& encoded = data["data"];

				if (type == "vector")
                    val = { glm::vec3(encoded[0], encoded[1], encoded[2]) };
				else
					RAISE_RTF("Unknown encoded datatype '{}'", type);
			}
			else
				val = jsonToGeneric(data);

            arr.push_back(val);
		}

        Reflection::GenericValue gv = arr;
        gv.Type = Reflection::ValueType::Map;

		return gv;
	}
	default:
	{
		assert(false);
        return std::format("< JSON Value : {} >", v.type_name());
	}
	}
}

#define ERROR_CONTEXTUALIZED_NVARARGS(e) { \
if (Context.size() > 0) \
	RAISE_RTF(e " (serializing {})", Context); \
else \
	RAISE_RT(e); } \

#define ERROR_CONTEXTUALIZED(e, ...) { \
if (Context.size() > 0) \
	RAISE_RTF(e " in {}", __VA_ARGS__, Context); \
else \
	RAISE_RTF(e, __VA_ARGS__); } \

static nlohmann::json genericToJson(const Reflection::GenericValue& Value, std::string Context = "")
{
    switch (Value.Type)
	{
	case Reflection::ValueType::Null:
		return {};

	case Reflection::ValueType::Boolean:
        return Value.AsBoolean();

	case Reflection::ValueType::Double:
        return (float)Value.AsDouble();

	case Reflection::ValueType::String:
        return Value.AsString();

    case Reflection::ValueType::Array:
    {
        nlohmann::json arr = nlohmann::json::array();
        for (uint32_t i = 0; i < Value.Size; i++)
            arr[i] = genericToJson(Value.Val.Array[i]);

        return arr;
    }

	case Reflection::ValueType::Map:
	{
		nlohmann::json t = nlohmann::json::object();
		Reflection::ValueType keytype = Reflection::ValueType::Null;

        std::span<Reflection::GenericValue> arr = Value.AsArray();

		for (uint32_t i = 0; i < arr.size(); i += 2)
		{
            const Reflection::GenericValue& k = arr[i];
            const Reflection::GenericValue& v = arr[i + 1];

			if (k.Type != keytype && keytype != Reflection::ValueType::Null)
			{
				ERROR_CONTEXTUALIZED(
					"All keys must have the same type. Previous type: {}, Current type: {} ('{}')",
					Reflection::TypeAsString(keytype), Reflection::TypeAsString(k.Type), k.ToString()
				);
			}

			if (keytype == Reflection::ValueType::Null)
			{
				keytype = k.Type;

				if (keytype != Reflection::ValueType::String)
				{
					ERROR_CONTEXTUALIZED(
						"Key type expected to be String, got '{}' ({})",
						k.ToString(), Reflection::TypeAsString(k.Type)
					);
				}
			}

			std::string key = k.AsString();

			if (key == JSON_ENCODED_DATA_TAG)
				ERROR_CONTEXTUALIZED_NVARARGS("The table key '" JSON_ENCODED_DATA_TAG "' is reserved");

			if (Context.size() == 0)
				Context = "Dictionary";

			t[key] = genericToJson(v, Context + "." + key);
		}

		return t;
	}

	case Reflection::ValueType::Vector3:
	{
		const glm::vec3& vec = Value.AsVector3();

		nlohmann::json value;
		nlohmann::json& data = value[JSON_ENCODED_DATA_TAG];
		data["type"] = "vector";
		data["data"][0] = vec[0];
		data["data"][1] = vec[1];
		data["data"][2] = vec[2];

		return value;
	}

	[[unlikely]] default:
	{
		ERROR_CONTEXTUALIZED(
			"Cannot convert '%s' (%s) to a JSON value",
			Value.ToString(), Reflection::TypeAsString(Value.Type)
		);
	}
	}
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
            REFLECTION_PROPERTY(
                "BoundDataModel",
                GameObject,
                [](void*) -> Reflection::GenericValue
                {
                    Engine* engine = Engine::Get();
                    return engine->DataModelRef->ToGenericValue();
                },
                nullptr
            )
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
                [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
                {
                    const auto& sels = InlayEditor::GetExplorerSelections();

                    std::vector<Reflection::GenericValue> out;
                    out.reserve(sels.size());

                    for (const ObjectHandle& obj : sels)
                        out.push_back(obj->ToGenericValue());

                    return { Reflection::GenericValue(out) };
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

                        const char* const ResultToMessage[] = {
                            "ok",
                            "yield",
                            nullptr,
                            nullptr,
                            nullptr,
                            nullptr,
                            "break"
                        };

                        std::string message = ResultToMessage[result] ? ResultToMessage[result] : lua_tostring(ML, -1);
                        lua_pop(vm.MainThread, 1); // pop off ML

                        return { result == LUA_OK, message };
	                }
	                else
                    {
                        std::string message = lua_tostring(ML, -1);
                        lua_pop(vm.MainThread, 1); // pop off ML

                        Log::Error(message, std::format("SourceChunkName:{}", chname));
                        return { false, message };
                    }
                }
            } },

            { "GetToolNames", Reflection::MethodDescriptor{
                {},
                { Reflection::ValueType::Array },
                [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
                {
                    std::vector<Reflection::GenericValue> out;
                    out.reserve(std::size(Tools));

                    for (const std::string_view& tool : Tools)
                        out.emplace_back(tool);

                    return { Reflection::GenericValue(out) };
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
                [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
                {
                    Engine* engine = Engine::Get();

                    std::vector<Reflection::GenericValue> out;
                    out.reserve(engine->argc);

                    for (int i = 0; i < engine->argc; i++)
                        out.emplace_back(engine->argv[i]);

                    return { Reflection::GenericValue(out) };
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
                [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
                {
                    Engine* engine = Engine::Get();
                    engine->OverrideDefaultViewport = false;
                    engine->ViewportPosition = {};

                    return {};
                }
            } },

            { "OpenTextDocument", Reflection::MethodDescriptor{
                { Reflection::ValueType::String, REFLECTION_OPTIONAL(Integer) },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    InlayEditor::OpenTextDocument(
                        inputs[0].AsString(),
                        inputs.size() > 1 ? (int)inputs[1].AsInteger() : 1
                    );

                    return {};
                }
            } },

            { "GetConfigValue", Reflection::MethodDescriptor{
                { Reflection::ValueType::String },
                { Reflection::ValueType::Any },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    return { jsonToGeneric(EngineJsonConfig[inputs[0].AsStringView()]) };
                }
            } },

            { "SetConfigValue", Reflection::MethodDescriptor{
                { Reflection::ValueType::String, Reflection::ValueType::Any },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    EngineJsonConfig[inputs[0].AsStringView()] = genericToJson(inputs[1]);

                    return {};
                }
            } },

            { "SaveConfig", Reflection::MethodDescriptor{
                {},
                { Reflection::ValueType::Boolean, Reflection::ValueType::String },
                [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
                {
                    std::string error;
                    bool writeSuccess = FileRW::WriteFile("./phoenix.conf", EngineJsonConfig.dump(2), &error);

                    return { writeSuccess, error };
                }
            } },

            { "PollEvents", Reflection::MethodDescriptor{
                {},
                {},
                [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
                {
                    glfwPollEvents();
                    return {};
                }
            } }
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

void EcEngine::SignalNewLogMessage(LogMessageType MessageType, const std::string_view& Message, const std::string_view& ExtraTags)
{
    for (const EcEngine& ce : Instance.m_Components)
    {
        if (ce.Valid)
            REFLECTION_SIGNAL_EVENT(ce.OnMessageLoggedCallbacks, (int)MessageType, Message, ExtraTags);
    }
}
