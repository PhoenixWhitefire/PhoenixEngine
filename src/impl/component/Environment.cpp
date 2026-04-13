// Environment.cpp, World ambience and lighting - 22/03/2026
#include "component/Environment.hpp"

const Reflection::StaticPropertyMap& EnvironmentComponentManager::GetProperties()
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
