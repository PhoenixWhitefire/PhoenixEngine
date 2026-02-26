#include <glm/gtc/matrix_transform.hpp>

#include "component/Light.hpp"
#include "component/Transform.hpp"

#define LIGHT_APIS(t) REFLECTION_PROPERTY_SIMPLE_NGV(t, LightColor, Color), \
REFLECTION_PROPERTY_SIMPLE(t, Brightness, Double) \

class PointLightManager : public ComponentManager<EcPointLight>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
            LIGHT_APIS(EcPointLight),

			REFLECTION_PROPERTY_SIMPLE(EcPointLight, Range, Double)
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

        return static_cast<uint32_t>(m_Components.size() - 1);
    }
    
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
            LIGHT_APIS(EcDirectionalLight),

            REFLECTION_PROPERTY_SIMPLE(EcDirectionalLight, Shadows, Boolean),

            REFLECTION_PROPERTY(
                "Direction",
                Vector3,
                [](void* p)
                -> Reflection::GenericValue
                {
                    return static_cast<EcDirectionalLight*>(p)->Direction;
                },
                [](void* p, const Reflection::GenericValue& gv)
                {
                    static_cast<EcDirectionalLight*>(p)->Direction = gv.AsVector3();
                }
            ),

            REFLECTION_PROPERTY_SIMPLE(EcDirectionalLight, ShadowViewOffset, Vector3),
            REFLECTION_PROPERTY_SIMPLE(EcDirectionalLight, ShadowViewDistance, Double),
            REFLECTION_PROPERTY_SIMPLE(EcDirectionalLight, ShadowViewSizeH, Double),
            REFLECTION_PROPERTY_SIMPLE(EcDirectionalLight, ShadowViewSizeV, Double),
			REFLECTION_PROPERTY_SIMPLE(EcDirectionalLight, ShadowViewNearPlane, Double),
			REFLECTION_PROPERTY_SIMPLE(EcDirectionalLight, ShadowViewFarPlane, Double),
			REFLECTION_PROPERTY_SIMPLE(EcDirectionalLight, ShadowViewMoveWithCamera, Boolean),

            REFLECTION_PROPERTY(
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
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
            LIGHT_APIS(EcSpotLight),

			REFLECTION_PROPERTY_SIMPLE(EcSpotLight, Range, Double),
			REFLECTION_PROPERTY_SIMPLE(EcSpotLight, Angle, Double)
        };

        return props;
    }
};

static inline PointLightManager PlInstance;
static inline DirectionalLightManager DlInstance;
static inline SpotLightManager SlInstance;
