// The Hierarchy Root
// 13/08/2024

#include "component/DataModel.hpp"
#include "Engine.hpp"
#include "Log.hpp"

class DataModelManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
        
        return static_cast<uint32_t>(m_Components.size() - 1);
    }
    
    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcDataModel& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

    virtual void* GetComponent(uint32_t Id) override
    {
        return &m_Components[Id];
    }

    virtual void DeleteComponent(uint32_t) override
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth
    }

    virtual const Reflection::PropertyMap& GetProperties() override
    {
        static const Reflection::PropertyMap props = {};

        return props;
    }

    virtual const Reflection::MethodMap& GetMethods() override
    {
        static const Reflection::MethodMap funcs =
		{
			{ "Close",
				{ 
					{ Reflection::ParameterType{ Reflection::ValueType::Integer, true } },
					{},
					[](void*, const std::vector<Reflection::GenericValue>& inputs)
                    -> std::vector<Reflection::GenericValue>
					{
                        Log::Info("Shutdown requested via DataModel");

                        Engine* engine = Engine::Get();
                        engine->ExitCode = inputs.size() > 0 ? static_cast<int32_t>(inputs[0].AsInteger()) : 0;
						engine->Close();

                        return {};
					}
				}
			}
		};
        return funcs;
    }

    virtual const Reflection::EventMap& GetEvents() override
    {
        static Reflection::EventMap events =
        {
            REFLECTION_EVENT(EcDataModel, OnFrameBegin, Reflection::ValueType::Double)
        };

        return events;
    }

    DataModelManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::DataModel] = this;
    }

private:
    std::vector<EcDataModel> m_Components;
};

static inline DataModelManager Instance{};
