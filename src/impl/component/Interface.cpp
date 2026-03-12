// UI, 12/03/2026

#include "component/Interface.hpp"

class InterfaceServiceManager : public ComponentManager<EcInterfaceService>
{
};

static InterfaceServiceManager InterfaceManager;

class UITransformManager : public ComponentManager<EcUITransform>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap& props = {
            REFLECTION_PROPERTY_SIMPLE(EcUITransform, Position, Vector2),
            REFLECTION_PROPERTY_SIMPLE(EcUITransform, Size, Vector2),
            REFLECTION_PROPERTY_SIMPLE(EcUITransform, Rotation, Double),
            REFLECTION_PROPERTY_SIMPLE(EcUITransform, ZIndex, Integer),
        };

        return props;
    }
};

static UITransformManager TransformManager;

class UIFrameManager : public ComponentManager<EcUIFrame>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap& props = {
            REFLECTION_PROPERTY_SIMPLE_NGV(EcUIFrame, BackgroundColor, Color),
            REFLECTION_PROPERTY_SIMPLE(EcUIFrame, BackgroundTransparency, Double)
        };

        return props;
    }
};

static UIFrameManager FramesManager;

class UIImageManager : public ComponentManager<EcUIImage>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap& props = {
            REFLECTION_PROPERTY_SIMPLE(EcUIImage, Image, String),
            REFLECTION_PROPERTY_SIMPLE_NGV(EcUIImage, ImageTint, Color),
            REFLECTION_PROPERTY_SIMPLE(EcUIImage, ImageTransparency, Double),
        };

        return props;
    }
};

static UIImageManager ImagesManager;

class UITextManager : public ComponentManager<EcUIText>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap& props = {
            REFLECTION_PROPERTY_SIMPLE(EcUIText, Text, String),
        };

        return props;
    }
};

static UITextManager TextManager;

class UIButtonManager : public ComponentManager<EcUIButton>
{
public:
    const Reflection::StaticEventMap& GetEvents() override
    {
        static const Reflection::StaticEventMap& events = {
            REFLECTION_EVENT(EcUIButton, OnClicked)
        };

        return events;
    }
};

static UIButtonManager ButtonManager;
