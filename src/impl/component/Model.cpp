#include "component/Model.hpp"
#include "datatype/GameObject.hpp"

class ModelManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) final
    {
        m_Components.emplace_back();

        if (!Object->GetComponentByType(EntityComponent::Transform))
            Object->AddComponent(EntityComponent::Transform);
        
        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() final
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcModel& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

    virtual void* GetComponent(uint32_t Id) final
	{
		return &m_Components[Id];
	}

    virtual void DeleteComponent(uint32_t) final
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth
    }

    virtual const Reflection::PropertyMap& GetProperties() final
    {
        static const Reflection::PropertyMap props = {};
        return props;
    }

    virtual const Reflection::FunctionMap& GetFunctions() final
    {
        static const Reflection::FunctionMap funcs = {};
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
