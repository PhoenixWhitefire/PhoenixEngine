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

    virtual const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
            LIGHT_APIS(EcPointLight),

			EC_PROP_SIMPLE(EcPointLight, Range, Double)
        };

        return props;
    }

    virtual const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap funcs = {};
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
                    static_cast<EcDirectionalLight*>(p)
                        ->Object->GetComponent<EcTransform>()->Transform[3] = glm::vec4(gv.AsVector3(), 1.f);
                }
            ),

            EC_PROP_SIMPLE(EcDirectionalLight, ShadowViewOffset, Vector3),
            EC_PROP_SIMPLE(EcDirectionalLight, ShadowViewDistance, Double),
            EC_PROP_SIMPLE(EcDirectionalLight, ShadowViewSizeH, Double),
            EC_PROP_SIMPLE(EcDirectionalLight, ShadowViewSizeV, Double),
			EC_PROP_SIMPLE(EcDirectionalLight, ShadowViewNearPlane, Double),
			EC_PROP_SIMPLE(EcDirectionalLight, ShadowViewFarPlane, Double),
			EC_PROP_SIMPLE(EcDirectionalLight, ShadowViewMoveWithCamera, Boolean),

            EC_PROP(
                "ShadowViewSize",
                Double,
                [](void* p)
                {
                    EcDirectionalLight* d = static_cast<EcDirectionalLight*>(p);

                    if (d->ShadowViewSizeH == d->ShadowViewSizeV)
                        return d->ShadowViewSizeH;
                    else
                        return -1.f;
                },
                [](void* p, const Reflection::GenericValue& gv)
                {
                    EcDirectionalLight* d = static_cast<EcDirectionalLight*>(p);

                    if (d->ShadowViewSizeH == d->ShadowViewSizeV)
                    {
                        float size = gv.AsDouble();
                        d->ShadowViewSizeH = size;
                        d->ShadowViewSizeV = size;
                    }
                }
            )
        };

        return props;
    }

    virtual const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap funcs = {};
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

    virtual const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
            LIGHT_APIS(EcSpotLight),

			EC_PROP_SIMPLE(EcSpotLight, Range, Double),
			EC_PROP_SIMPLE(EcSpotLight, Angle, Double)
        };

        return props;
    }

    virtual const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap funcs = {};
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
