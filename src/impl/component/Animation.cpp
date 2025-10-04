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
        static const Reflection::StaticPropertyMap props =
        {
            EC_PROP(
                "Animation",
                String,
                EC_GET_SIMPLE(EcAnimation, Animation),
                [](void* p, const Reflection::GenericValue& gv)
                {
                    static_cast<EcAnimation*>(p)->SetAnimation(gv.AsStringView());
                }
            ),

            EC_PROP_SIMPLE(EcAnimation, Weight, Double),
            EC_PROP_SIMPLE(EcAnimation, Playing, Boolean),
            EC_PROP_SIMPLE(EcAnimation, Looped, Boolean),
            EC_PROP("Ready", Boolean, EC_GET_SIMPLE(EcAnimation, Ready), nullptr)
        };
        return props;
    }
};

static inline AnimationManager Instance{};

void EcAnimation::SetAnimation(const std::string_view& Asset)
{
    bool found = false;
    std::string animFileContents = FileRW::ReadFile(std::string(Asset), &found);

    if (!found)
        RAISE_RTF("Cannot find animation file '{}'", Asset);

    nlohmann::json json;
    try
    {
        json = nlohmann::json::parse(animFileContents);
    }
    catch(const nlohmann::json::parse_error& e)
    {
        RAISE_RTF("Cannot parse animation file '{}': {}", Asset, e.what());
    }
    
    
}
