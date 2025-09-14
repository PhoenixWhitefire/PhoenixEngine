// The Hierarchy Root
// 13/08/2024

#include "component/DataModel.hpp"
#include "Log.hpp"

class DataModelManager : public ComponentManager<EcDataModel>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props =
        {
            EC_PROP(
                "Time",
                Double,
                [](void*)
                -> Reflection::GenericValue
                {
                    return GetRunningTime();
                },
                nullptr
            )
        };

        return props;
    }

// stupid compiler false positive warnings
#if defined(__GNUG__) && (__GNUG__ == 14)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

    const Reflection::StaticEventMap& GetEvents() override
    {
        static Reflection::StaticEventMap events =
        {
            REFLECTION_EVENT(EcDataModel, OnFrameBegin, Reflection::ValueType::Double)
        };

        return events;
    }

#if defined(__GNUG__) && (__GNUG__ == 14)
#pragma GCC diagnostic pop
#endif
};

static inline DataModelManager Instance{};
