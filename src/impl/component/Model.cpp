#include "component/Model.hpp"
#include "datatype/GameObject.hpp"

class ModelManager : public ComponentManager<EcModel>
{
    virtual const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
            REFLECTION_PROPERTY_SIMPLE(EcModel, ImportPath, String)
        };

        return props;
    }
};

static inline ModelManager Instance;
