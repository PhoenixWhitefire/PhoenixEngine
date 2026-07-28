// Luau userdata tag IDs, 27/05/2026
#pragma once

struct UserdataTag_
{
    enum UdT {
        __start = 1,

        Mutex = 1,
        SharedBuffer = 2,
        AtomicInteger = 3,
        Color = 4,
        EventSignal = 5,
        EventConnection = 6,
        GameObject = 7,
        InputEvent = 8,
        Matrix = 9,
        NumberGradient = 10,
        VectorGradient = 11,
        ColorGradient = 12,

        __count,
        __tag_limit = 127,
        __invalid = 128,
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
};

static_assert(std::size(UserdataTagNames) == UserdataTag::__count);
