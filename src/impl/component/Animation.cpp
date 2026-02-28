#include <vector>
#include <Vendor/nljson.hpp>

#include "component/Animation.hpp"
#include "datatype/GameObject.hpp"
#include "FileRW.hpp"

class AnimationManager : public ComponentManager<EcAnimation>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
            REFLECTION_PROPERTY(
                "Animation",
                String,
                REFLECTION_PROPERTY_GET_SIMPLE(EcAnimation, Animation),
                [](void* p, const Reflection::GenericValue& gv)
                {
                    static_cast<EcAnimation*>(p)->SetAnimation(gv.AsStringView());
                }
            ),

            REFLECTION_PROPERTY_SIMPLE(EcAnimation, Weight, Double),
            REFLECTION_PROPERTY_SIMPLE(EcAnimation, Playing, Boolean),
            REFLECTION_PROPERTY_SIMPLE(EcAnimation, Looped, Boolean),
            REFLECTION_PROPERTY("Ready", Boolean, REFLECTION_PROPERTY_GET_SIMPLE(EcAnimation, Ready), nullptr)
        };
        return props;
    }
};

static inline AnimationManager Instance;

void EcAnimation::SetAnimation(const std::string_view& Asset)
{
    if (Asset.size() == 0)
        return;

    bool found = false;
    std::string animFileContents = FileRW::ReadFile(std::string(Asset), &found);

    if (!found)
    {
        Log.ErrorF("Cannot find animation file '{}'", Asset);
        return;
    }

    nlohmann::json json;
    try
    {
        json = nlohmann::json::parse(animFileContents);
    }
    catch(const nlohmann::json::parse_error& e)
    {
        Log.ErrorF("Cannot parse animation file '{}': {}", Asset, e.what());
        return;
    }
    
    
}
