#include <vector>

#include "component/Animation.hpp"
#include "datatype/GameObject.hpp"

class AnimationManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject*) override
    {
        m_Components.emplace_back();
        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcAnimation& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

    virtual void* GetComponent(uint32_t Id) override
    {
        return (void*)&m_Components[Id];
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

    virtual const Reflection::FunctionMap& GetFunctions() override
    {
        static const Reflection::FunctionMap funcs = {};
        return funcs;
    }

    AnimationManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::Animation] = this;
    }
    
private:
    std::vector<EcAnimation> m_Components;
};

static inline AnimationManager Instance{};
