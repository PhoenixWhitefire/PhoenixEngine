// Luau userdata tag IDs, 27/05/2026
#pragma once

#include <string_view>

struct UserdataTag_
{
    enum UdT {
        start = 1,

        Mutex = 1,
        SharedBuffer,
        AtomicInteger,
        Color,
        EventSignal,
        EventConnection,
        GameObject,
        InputEvent,
        Matrix,
        NumberGradient,
        VectorGradient,
        ColorGradient,
        World,
        Query,
        Component,

        count,
        tagLimit = 127,
        invalid = 128,
    };
};

using UserdataTag = UserdataTag_::UdT;

const std::string_view UserdataTagNames[] = {
    "<UDINVALID>", // 0
    "Mutex",
    "SharedBuffer",
    "AtomicInteger",
    "Color",
    "EventSignal",
    "EventConnection",
    "GameObject",
    "InputEvent",
    "Matrix",
    "NumberGradient",
    "VectorGradient",
    "ColorGradient",
    "World",
    "Query",
    "Component",
};

static_assert(std::size(UserdataTagNames) == UserdataTag::count);
