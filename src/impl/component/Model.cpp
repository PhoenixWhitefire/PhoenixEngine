#include "component/Model.hpp"
#include "datatype/GameObject.hpp"

class ModelManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();

        if (!Object->GetComponentByType(EntityComponent::Transform))
            Object->AddComponent(EntityComponent::Transform);
        
        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcModel& t : m_Components)
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

    virtual const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {};
        return props;
    }

    virtual const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap funcs = {};
        return funcs;
    }

    ModelManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::Model] = this;
    }

private:
    std::vector<EcModel> m_Components;
};

static inline ModelManager Instance{};
