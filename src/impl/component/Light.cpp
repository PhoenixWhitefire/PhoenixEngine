#include <glm/gtc/matrix_transform.hpp>

#include "component/Light.hpp"
#include "component/Transform.hpp"

#define LIGHT_APIS(t) EC_PROP_SIMPLE_NGV(t, LightColor, Color), \
EC_PROP_SIMPLE(t, Brightness, Double), \
EC_PROP_SIMPLE(t, Shadows, Boolean) \

class PointLightManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
		
        if (!Object->GetComponent<EcTransform>())
		    Object->AddComponent(EntityComponent::Transform);

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcPointLight& t : m_Components)
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
        static const Reflection::PropertyMap props = 
        {
            LIGHT_APIS(EcPointLight),

			EC_PROP_SIMPLE(EcPointLight, Range, Double)
        };

        return props;
    }

    virtual const Reflection::MethodMap& GetMethods() override
    {
        static const Reflection::MethodMap funcs = {};
        return funcs;
    }

    PointLightManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::PointLight] = this;
    }

private:
    std::vector<EcPointLight> m_Components;
};

class DirectionalLightManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
        m_Components.back().Object = Object;

        if (!Object->GetComponent<EcTransform>())
		    Object->AddComponent(EntityComponent::Transform);

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcDirectionalLight& t : m_Components)
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

        m_Components[Id].Object.Invalidate();
    }

    virtual void Shutdown() override
    {
        for (size_t i = 0; i < m_Components.size(); i++)
            DeleteComponent(i);
    }

    virtual const Reflection::PropertyMap& GetProperties() override
    {
        static const Reflection::PropertyMap props = 
        {
            LIGHT_APIS(EcDirectionalLight),

            EC_PROP(
                "Direction",
                Vector3,
                [](void* p)
                -> Reflection::GenericValue
                {
                    return glm::vec3(static_cast<EcDirectionalLight*>(p)->Object->GetComponent<EcTransform>()->Transform[3]);
                },
                [](void* p, const Reflection::GenericValue& gv)
                {
                    static_cast<EcDirectionalLight*>(p)->Object->GetComponent<EcTransform>()->Transform[3] = glm::vec4(gv.AsVector3(), 1.f);
                }
            )
        };

        return props;
    }

    virtual const Reflection::MethodMap& GetMethods() override
    {
        static const Reflection::MethodMap funcs = {};
        return funcs;
    }

    DirectionalLightManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::DirectionalLight] = this;
    }

private:
    std::vector<EcDirectionalLight> m_Components;
};

class SpotLightManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
		
        if (!Object->GetComponent<EcTransform>())
		    Object->AddComponent(EntityComponent::Transform);

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcSpotLight& t : m_Components)
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
        static const Reflection::PropertyMap props = 
        {
            LIGHT_APIS(EcSpotLight),

			EC_PROP_SIMPLE(EcSpotLight, Range, Double),
			EC_PROP_SIMPLE(EcSpotLight, Angle, Double)
        };

        return props;
    }

    virtual const Reflection::MethodMap& GetMethods() override
    {
        static const Reflection::MethodMap funcs = {};
        return funcs;
    }

    SpotLightManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::SpotLight] = this;
    }

private:
    std::vector<EcSpotLight> m_Components;
};

static inline PointLightManager PlInstance{};
static inline DirectionalLightManager DlInstance{};
static inline SpotLightManager SlInstance{};
