// UI, 12/03/2026
#include "component/Interface.hpp"

const Reflection::StaticPropertyMap& UITransformComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap& props = {
        REFLECTION_PROPERTY_SIMPLE(EcUITransform, Position, Vector2),
        REFLECTION_PROPERTY_SIMPLE(EcUITransform, Size, Vector2),
        REFLECTION_PROPERTY_SIMPLE(EcUITransform, Rotation, Double),
        REFLECTION_PROPERTY_SIMPLE(EcUITransform, ZIndex, Integer),
    };

    return props;
}

const Reflection::StaticPropertyMap& UIFrameComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap& props = {
        REFLECTION_PROPERTY_SIMPLE_NGV(EcUIFrame, BackgroundColor, Color),
        REFLECTION_PROPERTY_SIMPLE(EcUIFrame, BackgroundTransparency, Double)
    };

    return props;
}

const Reflection::StaticPropertyMap& UIImageComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap& props = {
        REFLECTION_PROPERTY_SIMPLE(EcUIImage, Image, String),
        REFLECTION_PROPERTY_SIMPLE_NGV(EcUIImage, ImageTint, Color),
        REFLECTION_PROPERTY_SIMPLE(EcUIImage, ImageTransparency, Double),
    };

    return props;
}

const Reflection::StaticPropertyMap& UITextComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap& props = {
        REFLECTION_PROPERTY_SIMPLE(EcUIText, Text, String),
    };

    return props;
}

const Reflection::StaticEventMap& UIButtonComponentManager::GetEvents()
{
    static const Reflection::StaticEventMap& events = {
        REFLECTION_EVENT(EcUIButton, OnClicked)
    };

    return events;
}
