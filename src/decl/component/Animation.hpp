#pragma once

#include <string>

struct EcAnimation
{
    void SetAnimation(const std::string_view&);

    std::string Animation = "";
    float Weight = 1.f;

    bool Playing = true;
    bool Looped = false;
    bool Ready = false;
};

