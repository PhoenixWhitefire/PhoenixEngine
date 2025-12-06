#pragma once

#include <string>

#include "datatype/GameObject.hpp"

struct EcAnimation : public Component<EntityComponent::Animation>
{
    void SetAnimation(const std::string_view&);

    std::string Animation = "";
    float Weight = 1.f;

    bool Playing = true;
    bool Looped = false;
    bool Ready = false;
    bool Valid = true;
};
