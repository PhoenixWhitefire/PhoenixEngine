// DeveloperTools service, 31/03/2026
#include "component/DeveloperToolsService.hpp"
#include "Reflection.hpp"
#include "datatype/GameObject.hpp"
#include "DeveloperTools.hpp"
#include "Utilities.hpp"
#include "Log.hpp"

#define PROPERTY_PROXY(n) REFLECTION_PROPERTY( \
    #n, \
    Boolean, \
    [](void*) -> Reflection::GenericValue \
    { \
        return DeveloperTools::n; \
    }, \
    [](void*, const Reflection::GenericValue& gv) \
    { \
        DeveloperTools::n = gv.AsBoolean(); \
    } \
)

static const std::unordered_map<std::string_view, bool*> Tools = {
    { "Explorer", &DeveloperTools::ExplorerShown },
    { "Properties", &DeveloperTools::PropertiesShown },
    { "Materials", &DeveloperTools::MaterialsShown },
    { "Shaders", &DeveloperTools::ShadersShown },
    { "Renderer", &DeveloperTools::RendererShown },
    { "Info", &DeveloperTools::InfoShown },
    { "Scripts", &DeveloperTools::ScriptsShown },
    { "Documentation", &DeveloperTools::DocumentationShown }
};

static bool* getTool(const std::string_view& toolName)
{
    for (const auto& [ name, shownPtr ] : Tools)
    {
        if (name == toolName)
            return shownPtr;
    }

    RAISE_RT("Invalid tool '{}'", toolName);
}

void DeveloperToolsComponentManager::BindService(uint32_t)
{
}

void DeveloperToolsComponentManager::UnbindService()
{
}

const Reflection::StaticPropertyMap& DeveloperToolsComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY(
            "Initialized",
            Boolean,
            [](void*) -> Reflection::GenericValue
            {
                return DeveloperTools::Initialized;
            },
            nullptr
        ),

        PROPERTY_PROXY(DocumentationShown),
        PROPERTY_PROXY(ExplorerShown),
        PROPERTY_PROXY(InfoShown),
        PROPERTY_PROXY(MaterialsShown),
        PROPERTY_PROXY(PropertiesShown),
        PROPERTY_PROXY(RendererShown),
        PROPERTY_PROXY(ScriptsShown),
        PROPERTY_PROXY(SettingsShown),
        PROPERTY_PROXY(ShadersShown),
    };

    return props;
}

const Reflection::StaticMethodMap& DeveloperToolsComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap methods = {
        { "Initialize", Reflection::MethodDescriptor{
            {},
            {},
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                if (DeveloperTools::Initialized)
                {
                    Log.Warning("Called `DeveloperTools:Initialize` but they were already initialized");
                    return {};
                }

                DeveloperTools::Initialize(Renderer::Get());
                return {};
            }
        } },

        { "SetExplorerRoot", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::GameObject }),
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                DeveloperTools::SetExplorerRoot(GameObjectManager::Get()->FromGenericValue(inputs[0]));

                return {};
            }
        } },

        { "SetExplorerSelections", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::Array }),
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const std::span<Reflection::GenericValue>& inner = inputs[0].AsArray();

                std::vector<ObjectHandle> objects;
                objects.reserve(inner.size());

                for (const Reflection::GenericValue& gv : inner)
                    objects.emplace_back(GameObjectManager::Get()->FromGenericValue(gv));

                DeveloperTools::SetExplorerSelections(objects);
                return {};
            }
        } },

        { "GetExplorerSelections", Reflection::MethodDescriptor{
            {},
            REFLECTION_SPAN({ Reflection::ValueType::Array }),
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                const auto& sels = DeveloperTools::GetExplorerSelections();

                std::vector<Reflection::GenericValue> out;
                out.reserve(sels.size());

                for (const ObjectHandle& obj : sels)
                    out.push_back(obj->ToGenericValue());

                return { Reflection::GenericValue(out) };
            }
        } },

        { "GetToolNames", Reflection::MethodDescriptor{
            {},
            REFLECTION_SPAN({ Reflection::ValueType::Array }),
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                std::vector<Reflection::GenericValue> out;
                out.reserve(std::size(Tools));

                for (const auto& [ tool, _ ] : Tools)
                    out.emplace_back(tool);

                return { Reflection::GenericValue(out) };
            }
        } },

        { "SetToolEnabled", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String, Reflection::ValueType::Boolean }),
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                bool* enabledPtr = getTool(inputs[0].AsStringView());
                *enabledPtr = inputs[1].AsBoolean();

                return {};
            }
        } },

        { "IsToolEnabled", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            REFLECTION_SPAN({ Reflection::ValueType::Boolean }),
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                return { *getTool(inputs[0].AsStringView()) };
            }
        } },

        { "OpenTextDocument", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String, REFLECTION_OPTIONAL(Integer), REFLECTION_OPTIONAL(Boolean) }),
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                DeveloperTools::OpenTextDocument(
                    inputs[0].AsString(),
                    inputs.size() > 1 ? (int)inputs[1].AsInteger() : 1,
                    inputs.size() > 2 ? inputs[2].AsBoolean() : true
                );

                return {};
            }
        } },

        { "SaveTextDocuments", Reflection::MethodDescriptor{
            {},
            {},
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                DeveloperTools::SaveTextDocuments();
                return {};
            }
        } },

        { "CloseTextDocuments", Reflection::MethodDescriptor{
            {},
            {},
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                DeveloperTools::CloseTextDocuments();
                return {};
            }
        } },

        { "GetOpenTextDocuments", Reflection::MethodDescriptor{
            {},
            REFLECTION_SPAN({ Reflection::ValueType::Array }),
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                const std::vector<std::string_view> opened = DeveloperTools::GetOpenTextDocuments();
                std::vector<Reflection::GenericValue> ret;
                ret.reserve(opened.size());

                for (const std::string_view& path : opened)
                    ret.emplace_back(path);

                return { Reflection::GenericValue(ret) };
            }
        } },

        { "GetDocumentBreakpoints", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            REFLECTION_SPAN({ Reflection::ValueType::Array }),
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const std::vector<DebugBreakpoint> breakpoints = DeveloperTools::GetDocumentBreakpoints(inputs[0].AsString());
                std::vector<Reflection::GenericValue> rets;
                rets.reserve(breakpoints.size());

                for (const DebugBreakpoint& bp : breakpoints)
                {
                    rets.push_back(Reflection::GenericValue::MapPairs({
                        { "Line", bp.Line },
                        { "Enabled", bp.Enabled },
                        { "ConditionEnabled", bp.ConditionEnabled },
                        { "Condition", bp.Condition },
                    }));
                }

                return { Reflection::GenericValue(std::span<Reflection::GenericValue>(rets)) };
            }
        } },

        { "SetDocumentBreakpoints", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String, Reflection::ValueType::Array }),
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const std::string& file = inputs[0].AsString();
                const std::span<Reflection::GenericValue>& bparr = inputs[1].AsArray();

                std::vector<DebugBreakpoint> breakpoints;
                breakpoints.reserve(bparr.size());

                for (const Reflection::GenericValue& bpg : bparr)
                {
                    DebugBreakpoint bp;

                    bpg.ForEachMapPair([&](const Reflection::GenericValue& key, const Reflection::GenericValue& value)
                    {
                        const std::string_view& field = key.AsStringView();

                        if (field == "Line")
                            bp.Line = (int)value.AsInteger();
                        else if (field == "Enabled")
                            bp.Enabled = value.AsBoolean();
                        else if (field == "ConditionEnabled")
                            bp.ConditionEnabled = value.AsBoolean();
                        else if (field == "Condition")
                            bp.Condition = value.AsString();
                        else
                            RAISE_RT("Invalid field '{}' with value '{}' ()", field, value.ToString(), Reflection::TypeAsString(value.Type));
                    });

                    breakpoints.push_back(bp);
                }

                DeveloperTools::SetDocumentBreakpoints(file, breakpoints);
                return {};
            }
        } },
    };

    return methods;
}

const Reflection::StaticEventMap& DeveloperToolsComponentManager::GetEvents()
{
    static const Reflection::StaticEventMap events = {
        REFLECTION_EVENT(EcDeveloperToolsService, BreakpointUpdated, Reflection::ValueType::String, Reflection::ValueType::Map),
        REFLECTION_EVENT(EcDeveloperToolsService, BreakpointRemoved, Reflection::ValueType::String, Reflection::ValueType::Integer),

        REFLECTION_EVENT(EcDeveloperToolsService, DebuggerRequestedStop),
    };

    return events;
}
