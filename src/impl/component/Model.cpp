#include "component/Model.hpp"

const Reflection::StaticPropertyMap& ModelComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY_SIMPLE(EcModel, ImportPath, String)
    };

    return props;
}
