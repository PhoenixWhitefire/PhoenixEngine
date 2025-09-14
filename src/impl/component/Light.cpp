#include <glm/gtc/matrix_transform.hpp>

#include "component/Light.hpp"
#include "component/Transform.hpp"

#define LIGHT_APIS(t) EC_PROP_SIMPLE_NGV(t, LightColor, Color), \
EC_PROP_SIMPLE(t, Brightness, Double), \
EC_PROP_SIMPLE(t, Shadows, Boolean) \

class PointLightManager : public ComponentManager<EcPointLight>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
		
        if (!Object->GetComponent<EcTransform>())
		    Object->AddComponent(EntityComponent::Transform);

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
            LIGHT_APIS(EcPointLight),

			EC_PROP_SIMPLE(EcPointLight, Range, Double)
        };

        return props;
    }
};

class DirectionalLightManager : public ComponentManager<EcDirectionalLight>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
        m_Components.back().Object = Object;

        if (!Object->GetComponent<EcTransform>())
		    Object->AddComponent(EntityComponent::Transform);

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    void DeleteComponent(uint32_t Id) override
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth
        m_Components[Id].Object.~GameObjectRef();

        ComponentManager<EcDirectionalLight>::DeleteComponent(Id);
    }

    const Reflection::StaticPropertyMap& GetProperties() override
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
                -> Reflection::GenericValue
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
};

class SpotLightManager : public ComponentManager<EcSpotLight>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
		
        if (!Object->GetComponent<EcTransform>())
		    Object->AddComponent(EntityComponent::Transform);

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
            LIGHT_APIS(EcSpotLight),

			EC_PROP_SIMPLE(EcSpotLight, Range, Double),
			EC_PROP_SIMPLE(EcSpotLight, Angle, Double)
        };

        return props;
    }
};

static inline PointLightManager PlInstance{};
static inline DirectionalLightManager DlInstance{};
static inline SpotLightManager SlInstance{};
