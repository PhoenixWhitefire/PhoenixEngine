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

class DeveloperToolsServiceManager : public ComponentManager<EcDeveloperToolsService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
            REFLECTION_PROPERTY(
                "Initialzed",
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

    const Reflection::StaticMethodMap& GetMethods() override
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
                        return;
                    }

                    DeveloperTools::Initialize(Renderer::Get());
                    return {};
                }
            } }
        };

        return methods;
    }
};
