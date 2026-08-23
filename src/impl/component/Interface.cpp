// UI, 12/03/2026
#include <glad/gl.h>

#define GLT_IMPORTS
#include <glText/gltext.h>

#include "component/Interface.hpp"
#include "Engine.hpp"

const Reflection::StaticPropertyMap& InterfaceComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {};

    return props;
}

const Reflection::StaticPropertyMap& UITransformComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY_SIMPLE(EcUITransform, Position, Vector2),
        REFLECTION_PROPERTY_SIMPLE(EcUITransform, Size, Vector2),
        REFLECTION_PROPERTY_SIMPLE(EcUITransform, Rotation, Double),
        REFLECTION_PROPERTY_SIMPLE(EcUITransform, ZIndex, Integer),
    };

    return props;
}

const Reflection::StaticPropertyMap& UIFrameComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY_SIMPLE_NGV(EcUIFrame, BackgroundColor, Color),
        REFLECTION_PROPERTY_SIMPLE(EcUIFrame, BackgroundTransparency, Double),
    };

    return props;
}

const Reflection::StaticPropertyMap& UIImageComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY_SIMPLE(EcUIImage, Image, String),
        REFLECTION_PROPERTY_SIMPLE_NGV(EcUIImage, ImageTint, Color),
        REFLECTION_PROPERTY_SIMPLE(EcUIImage, ImageTransparency, Double),
    };

    return props;
}

const Reflection::StaticPropertyMap& UITextComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY(
            "Text",
            String,
            [](void* p) -> Reflection::GenericValue
            {
                return static_cast<EcUIText*>(p)->Text;
            },
            [](void* p, const Reflection::GenericValue& gv)
            {
                EcUIText* ui = static_cast<EcUIText*>(p);
                ui->Text = gv.AsString();

                if (!ui->Data)
                    return;

                gltSetText(ui->Data, ui->Text.c_str());
            }
        ),

        REFLECTION_PROPERTY_SIMPLE_NGV(EcUIText, TextColor, Color),
        REFLECTION_PROPERTY_SIMPLE(EcUIText, TextTransparency, Double),
    };

    return props;
}

uint32_t UITextComponentManager::CreateComponent(GameObject* Object)
{
    uint32_t id = ComponentManager<EcUIText>::CreateComponent(Object);

    if (!Engine::Get()->IsHeadlessMode)
    {
        EcUIText& uti = Components[id];
        uti.Data = gltCreateText();
        gltSetText(uti.Data, uti.Text.c_str());
    }

    return id;
}

void UITextComponentManager::DeleteComponent(uint32_t Id)
{
    if (EcUIText& uti = Components[Id]; uti.Data)
    {
        gltDeleteText(uti.Data);
        uti.Data = nullptr;
    }

    ComponentManager<EcUIText>::DeleteComponent(Id);
}

const Reflection::StaticPropertyMap& UIButtonComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY_SIMPLE(EcUIButton, IsClicking, Boolean),
        REFLECTION_PROPERTY_SIMPLE(EcUIButton, IsHovering, Boolean),
    };

    return props;
}

const Reflection::StaticEventMap& UIButtonComponentManager::GetEvents()
{
    static const Reflection::StaticEventMap events = {
        REFLECTION_EVENT(EcUIButton, ClickChanged, Reflection::ValueType::Boolean),
        REFLECTION_EVENT(EcUIButton, HoverChanged, Reflection::ValueType::Boolean),
    };

    return events;
}
