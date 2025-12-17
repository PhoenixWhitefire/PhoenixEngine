// The Hierarchy Root
// 13/08/2024

#include "component/DataModel.hpp"
#include "Log.hpp"

class DataModelManager : public ComponentManager<EcDataModel>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        uint32_t id = ComponentManager<EcDataModel>::CreateComponent(Object);
        m_Components.back().Object = Object;

        return id;
    }

    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
            REFLECTION_PROPERTY(
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

    const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap methods = {
            { "GetService", {
                { Reflection::ValueType::String },
                { Reflection::ValueType::GameObject },
                [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    const std::string_view Services[] = {
                        "Workspace",
                        "Engine",
                        "InputService",
                        "AssetService",
                        "HistoryService"
                    };

                    std::string service = std::string(inputs[0].AsStringView());
                    bool validService = false;

                    for (const std::string_view& serv : Services)
                        if (serv == service)
                        {
                            validService = true;
                            break;
                        }

                    if (!validService)
                        RAISE_RTF("'{}' is not a valid Service!", service);

                    EcDataModel* dm = static_cast<EcDataModel*>(p);
                    const auto& it = s_ComponentNameToType.find(service);

                    if (it == s_ComponentNameToType.end())
                        RAISE_RTF("Could not map Service name '{}' to a Component", service);

                    if (GameObject* preExisting = dm->Object->FindChildWithComponent(it->second))
                        return { preExisting->ToGenericValue() };

                    GameObject* newObject = GameObject::Create(it->second);
                    newObject->SetParent(dm->Object);

                    return { newObject->ToGenericValue() };
                }
            } }
        };

        return methods;
    }

    const Reflection::StaticEventMap& GetEvents() override
    {
        static Reflection::StaticEventMap events = {
            REFLECTION_EVENT(EcDataModel, OnFrameBegin, Reflection::ValueType::Double)
        };

        return events;
    }
};

static inline DataModelManager Instance;
