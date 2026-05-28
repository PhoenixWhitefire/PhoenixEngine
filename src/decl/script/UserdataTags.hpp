// Luau userdata tag IDs, 27/05/2026
#pragma once

struct UserdataTag_
{
    enum UdT {
        __start = 1,

        Mutex = 1,
        SharedBuffer = 2,
        AtomicInteger = 3,

        __count
    };
};

using UserdataTag = UserdataTag_::UdT;

const std::string_view UserdataTagNames[] = {
    "<INVALID>", // 0
    "Mutex",
    "SharedBuffer",
    "AtomicInteger",
};

static_assert(std::size(UserdataTagNames) == UserdataTag::__count);
