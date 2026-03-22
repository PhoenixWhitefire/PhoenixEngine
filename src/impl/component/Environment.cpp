// Environment.cpp, World ambience and lighting - 22/03/2026
#include "component/Environment.hpp"

class EnvironmentServiceManager : public ComponentManager<EcEnvironmentService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
            REFLECTION_PROPERTY_SIMPLE_NGV(EcEnvironmentService, AmbientLight, Color),
            REFLECTION_PROPERTY_SIMPLE(EcEnvironmentService, Fog, Boolean),
            REFLECTION_PROPERTY_SIMPLE_NGV(EcEnvironmentService, FogColor, Color),
            REFLECTION_PROPERTY_SIMPLE(EcEnvironmentService, PostProcess, Boolean),
            REFLECTION_PROPERTY_SIMPLE(EcEnvironmentService, GammaCorrection, Double),
        };

        return props;
    }
};

static EnvironmentServiceManager Instance;
