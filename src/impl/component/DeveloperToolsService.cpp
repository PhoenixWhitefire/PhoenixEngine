// DeveloperTools service, 31/03/2026
#include "component/DeveloperToolsService.hpp"
#include "DeveloperTools.hpp"

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

    RAISE_RTF("Invalid tool '{}'", toolName);
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
            { Reflection::ValueType::GameObject },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                DeveloperTools::SetExplorerRoot(GameObject::FromGenericValue(inputs[0]));

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

                DeveloperTools::SetExplorerSelections(objects);
                return {};
            }
        } },

        { "GetExplorerSelections", Reflection::MethodDescriptor{
            {},
            { Reflection::ValueType::Array },
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
            { Reflection::ValueType::Array },
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
            { Reflection::ValueType::String, Reflection::ValueType::Boolean },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                bool* enabledPtr = getTool(inputs[0].AsStringView());
                *enabledPtr = inputs[1].AsBoolean();

                return {};
            }
        } },

        { "IsToolEnabled", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            { Reflection::ValueType::Boolean },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                return { *getTool(inputs[0].AsStringView()) };
            }
        } },

        { "OpenTextDocument", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, REFLECTION_OPTIONAL(Integer) },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                DeveloperTools::OpenTextDocument(
                    inputs[0].AsString(),
                    inputs.size() > 1 ? (int)inputs[1].AsInteger() : 1
                );

                return {};
            }
        } },
    };

    return methods;
}
