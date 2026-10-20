// BuiltinComponents.hpp, 20/10/2026
#pragma once

#include <string_view>
#include "Ecs.hpp"

struct Ec_
{
    enum EC : Id
    {
        start = 1,

        Wildcard = 1,
        Name = 2,
        ChildOf = 3,
        Transform = 4,

        afterEnd,
        count = afterEnd - 1
    };
};

using Ec = Ec_::EC;

constexpr std::string_view EcNames[] = {
    "Wildcard",
    "Name",
    "ChildOf",
    "Transform",
};

static_assert(std::size(EcNames) == Ec::count);
