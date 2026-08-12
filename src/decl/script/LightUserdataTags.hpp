// Luau light userdata tag IDs, 28/07/2026
#pragma once

struct LightUserdataTag_
{
    enum LUdT {
        start = 1,

        RequirerContext = 1,
        GameObjectRawEquality = 2,
        GameObjectMethod = 3,
        SharedMutexRawEquality = 4,
        SharedBufferRawEquality = 5,
        AtomicIntegerRawEquality = 6,
        EventConnectionData = 7,

        count,
        tagLimit = 127,
        invalid = 128,
    };
};

using LightUserdataTag = LightUserdataTag_::LUdT;

const std::string_view LightUserdataTagNames[] = {
    "<LUINVALID>", // 0
    "RequirerContext",
    "GameObjectRawEquality",
    "GameObjectMethod",
    "SharedMutexRawEquality",
    "SharedBufferRawEquality",
    "AtomicIntegerRawEquality",
    "EventConnectionData",
};

static_assert(std::size(LightUserdataTagNames) == LightUserdataTag::count);
