#include <glm/gtc/matrix_transform.hpp>

#include "component/Light.hpp"
#include "component/Transform.hpp"

#define LIGHT_APIS(t) EC_PROP_SIMPLE_NGV(t, LightColor, Color), \
EC_PROP_SIMPLE(t, Brightness, Double), \
EC_PROP_SIMPLE(t, Shadows, Boolean) \

class PointLightManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) final
    {
        m_Components.emplace_back();
		
        if (!Object->GetComponent<EcTransform>())
		    Object->AddComponent(EntityComponent::Transform);

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() final
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcPointLight& t : m_Components)
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
        static const Reflection::PropertyMap props = 
        {
            LIGHT_APIS(EcPointLight),

			EC_PROP_SIMPLE(EcPointLight, Range, Double)
        };

        return props;
    }

    virtual const Reflection::FunctionMap& GetFunctions() final
    {
        static const Reflection::FunctionMap funcs = {};
        return funcs;
    }

    PointLightManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::PointLight] = this;
    }

private:
    std::vector<EcPointLight> m_Components;
};

class DirectionalLightManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) final
    {
        m_Components.emplace_back();
        m_Components.back().Object = Object;

        if (!Object->GetComponent<EcTransform>())
		    Object->AddComponent(EntityComponent::Transform);

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() final
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcDirectionalLight& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

    virtual void* GetComponent(uint32_t Id) final
	{
		return &m_Components[Id];
	}

    virtual void DeleteComponent(uint32_t Id) final
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth

        m_Components[Id].Object.Invalidate();
    }

    virtual void Shutdown() final
    {
        for (size_t i = 0; i < m_Components.size(); i++)
            DeleteComponent(i);
    }

    virtual const Reflection::PropertyMap& GetProperties() final
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

    virtual const Reflection::FunctionMap& GetFunctions() final
    {
        static const Reflection::FunctionMap funcs = {};
        return funcs;
    }

    DirectionalLightManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::DirectionalLight] = this;
    }

private:
    std::vector<EcDirectionalLight> m_Components;
};

class SpotLightManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) final
    {
        m_Components.emplace_back();
		
        if (!Object->GetComponent<EcTransform>())
		    Object->AddComponent(EntityComponent::Transform);

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() final
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcSpotLight& t : m_Components)
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
        static const Reflection::PropertyMap props = 
        {
            LIGHT_APIS(EcSpotLight),

			EC_PROP_SIMPLE(EcSpotLight, Range, Double),
			EC_PROP_SIMPLE(EcSpotLight, Angle, Double)
        };

        return props;
    }

    virtual const Reflection::FunctionMap& GetFunctions() final
    {
        static const Reflection::FunctionMap funcs = {};
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

#if 0

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Light);
PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(DirectionalLight);
PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(PointLight);
PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(SpotLight);

static bool s_BaseDidInitReflection = false;
static bool s_DirectionalDidInitReflection = false;
static bool s_PointDidInitReflection = false;
static bool s_SpotDidInitReflection = false;

void Object_Light::s_DeclareReflections()
{
	if (s_BaseDidInitReflection)
		return;
	s_BaseDidInitReflection = true;

	REFLECTION_INHERITAPI(Attachment);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Light, LightColor, Color);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Light, Brightness, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Light, Shadows, Bool);
}

void Object_DirectionalLight::s_DeclareReflections()
{
	if (s_DirectionalDidInitReflection)
		return;
	s_DirectionalDidInitReflection = true;

	// 27/11/2024 It's... PERFECT
	REFLECTION_INHERITAPI(Light);

	// we remove all the smelly `Attachment` properties we inherit from daddy `Light` 
	for (auto& propIt : Object_Attachment::s_GetProperties())
		s_Api.Properties.erase(propIt.first);
	for (auto& funcIt : Object_Attachment::s_GetFunctions())
		s_Api.Properties.erase(funcIt.first);

	// , before RE-INHERITING Father `GameObject` so we have the Base APIs
	// ( Name, ClassName, Enabled etc )
	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROP(
		"Direction",
		Vector3,
		[](Reflection::Reflectable* g)
		{
			Object_DirectionalLight* dl = static_cast<Object_DirectionalLight*>(g);
			glm::vec3 forward = dl->LocalTransform[3];
			return Vector3(forward).ToGenericValue(); //Vector3(forward / glm::length(forward)).ToGenericValue();
		},
		[](Reflection::Reflectable* g, const Reflection::GenericValue& gv)
		{
			Object_DirectionalLight* p = static_cast<Object_DirectionalLight*>(g);

			//Vector3 newDirection{ gv };
			//newDirection = newDirection / newDirection.Magnitude();

			p->LocalTransform = glm::translate(glm::mat4(1.f), (glm::vec3)Vector3(gv));
		}
	);
	
}

void Object_PointLight::s_DeclareReflections()
{
	if (s_PointDidInitReflection)
		return;
	s_PointDidInitReflection = true;

	REFLECTION_INHERITAPI(Light);

	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_PointLight, Range, Double, float);
}

void Object_SpotLight::s_DeclareReflections()
{
	if (s_SpotDidInitReflection)
		return;
	s_SpotDidInitReflection = true;

	REFLECTION_INHERITAPI(Light);

	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_SpotLight, Range, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_SpotLight, Angle, Double, float);
}

Object_Light::Object_Light()
{
	this->Name = "Light";
	this->ClassName = "Light";

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

Object_DirectionalLight::Object_DirectionalLight()
{
	this->Name = "DirectionalLight";
	this->ClassName = "DirectionalLight";

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

Object_PointLight::Object_PointLight()
{
	this->Name = "PointLight";
	this->ClassName = "PointLight";

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

Object_SpotLight::Object_SpotLight()
{
	this->Name = "SpotLight";
	this->ClassName = "SpotLight";

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

#endif
