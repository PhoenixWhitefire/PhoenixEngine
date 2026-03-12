// UI, 12/03/2026
#pragma once

#include "datatype/ComponentBase.hpp"
#include "datatype/Color.hpp"

struct EcInterfaceService : public Component<EntityComponent::Interface>
{
    bool Valid = true;
};

struct EcUITransform : public Component<EntityComponent::UITransform>
{
    glm::vec2 Position;
    glm::vec2 Size = { 1.f, 1.f };
    float Rotation = 0.f;
    int ZIndex = 0;

    bool Valid = true;
};

struct EcUIFrame : public Component<EntityComponent::UIFrame>
{
    Color BackgroundColor = { 1.f, 1.f, 1.f };
    float BackgroundTransparency = 0.f;

    bool Valid = true;
};

struct EcUIImage : public Component<EntityComponent::UIImage>
{
    std::string Image = "textures/image-placeholder.png";
    Color ImageTint = { 1.f, 1.f, 1.f };
    float ImageTransparency = 0.f;
    uint32_t ImageId = 0;

    bool Valid = true;
};

struct EcUIText : public Component<EntityComponent::UIText>
{
    std::string Text;

    bool Valid = true;
};

struct EcUIButton : public Component<EntityComponent::UIButton>
{
    std::vector<Reflection::EventCallback> OnClickedCallbacks;

    bool Valid = true;
};
