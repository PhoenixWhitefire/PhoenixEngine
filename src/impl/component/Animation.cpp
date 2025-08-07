#include <vector>
#include <Vendor/nljson.hpp>

#include "component/Animation.hpp"
#include "datatype/GameObject.hpp"
#include "FileRW.hpp"

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

    virtual const Reflection::StaticPropertyMap& GetProperties() override
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

    virtual const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap funcs = {};
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

void EcAnimation::SetAnimation(const std::string_view& Asset)
{
    bool found = false;
    std::string animFileContents = FileRW::ReadFile(Asset, &found);

    if (!found)
        RAISE_RT(std::format("Cannot find animation file '{}'", Asset));

    nlohmann::json json;
    try
    {
        json = nlohmann::json::parse(animFileContents);
    }
    catch(const nlohmann::json::parse_error& e)
    {
        RAISE_RT(std::format("Cannot parse animation file '{}': {}", Asset, e.what()));
    }
    
    
}
