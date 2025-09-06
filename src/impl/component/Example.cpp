// 15/09/2024 PHOENIXWHITEFIRE2000
// A template/example GameObject.

#include <tracy/public/tracy/Tracy.hpp>

#include "component/Example.hpp"
#include "datatype/GameObject.hpp"
#include "Engine.hpp"

class ExamplesManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
		m_Components.back().Object = Object;

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcExample& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

    virtual void* GetComponent(uint32_t Id) override
    {
        return &m_Components[Id];
    }

    virtual void DeleteComponent(uint32_t Id) override
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth

        m_Components[Id].Object.~GameObjectRef();
    }

	virtual void Shutdown() override
	{
		m_Components.clear();
	}

    virtual const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
            EC_PROP(
                "SuperCoolBool",
                Boolean,
				[](void* p)
				-> Reflection::GenericValue
				{
					EcExample* example = static_cast<EcExample*>(p);
					return example->SuperCoolBool;
				},
                [](void* p, const Reflection::GenericValue& gv)
                {
                    EcExample* example = static_cast<EcExample*>(p);
					example->SuperCoolBool = gv.AsBoolean();
                }
            ),

			// Effectively equivalent to the above
			EC_PROP_SIMPLE(EcExample, SomeInteger, Integer),
			EC_PROP_SIMPLE(EcExample, WhereIAm, Vector3),
			// A read-only property
			EC_PROP("SecretMessage", String, EC_GET_SIMPLE(EcExample, SecretMessage), nullptr),
			// A property
			EC_PROP("EvenMoreSecretMessage", String, nullptr, nullptr)
        };

        return props;
    }

    virtual const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap funcs =
		{
			{ "Greet", {
				{ Reflection::ValueType::Array, Reflection::ValueType::Boolean },
				{ Reflection::ValueType::String, Reflection::ValueType::String },
				[](void* p, const std::vector<Reflection::GenericValue>& args)
				-> std::vector<Reflection::GenericValue>
				{
					EcExample* ex = static_cast<EcExample*>(p);
					GameObjectRef object = ex->Object;

					Reflection::GenericValue gv = args.at(0);
					std::span<Reflection::GenericValue> names = gv.AsArray();

					if (names.empty())
						return { object->Name + ": He- Oh... There's no one here... :(" };

					else if (names.size() == 1)
						return { object->Name + ": Hello, " + std::string(names[0].AsStringView()) + "!" };

					std::string result = "Hello, ";

					for (size_t index = 0; index < names.size() - 1; index++)
						result += std::string(names[index].AsStringView()) + ", ";

					result = result.substr(0, result.size() - 2);
					result += " and " + std::string(names.back().AsStringView()) + "!";

					if (args[1].AsBoolean())
						result += " (MANIACAL CACKLING) HAHAHAHAHAHAHA";

					std::vector<Reflection::GenericValue> returnVals =
					{
						Reflection::GenericValue(object->Name),
						Reflection::GenericValue(result)
					};

					REFLECTION_SIGNAL(ex->OnGreetedCallbacks, returnVals);

					return returnVals;
				}
			} },

			{ "GiveUp", {
				{},
				{},
				[](void* p, const std::vector<Reflection::GenericValue>&)
				-> std::vector<Reflection::GenericValue>
				{
					static_cast<EcExample*>(p)->Object->Destroy();
					Engine::Get()->Close();

					RAISE_RT("learning is too harddddddddddddd >///~///<");
				}
			} }
		};

        return funcs;
    }

	virtual const Reflection::StaticEventMap& GetEvents() override
	{
		static const Reflection::StaticEventMap events =
		{
			REFLECTION_EVENT(EcExample, OnGreeted, Reflection::ValueType::String, Reflection::ValueType::String)
		};

		return events;
	}

    ExamplesManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::Example] = this;
    }

private:
    std::vector<EcExample> m_Components;
};

static inline ExamplesManager Instance{};
