// Reserved texture slots, 11/05/2026
#pragma once

#include <cstdint>

// can't use enums because of sign promotion with `Reflection::GenericValue`
struct ReservedTextureSlot
{
    static constexpr uint32_t Framebuffer = 0;
    static constexpr uint32_t PostProcessFramebuffer = 1;
    static constexpr uint32_t BloomSourceBuffer = 2;
    static constexpr uint32_t BloomResultBuffer = 3;

    static constexpr uint32_t SkyboxCubemap = 5;
    static constexpr uint32_t SkyboxEquirectangular = 6;
    static constexpr uint32_t Shadowmap = 7;
    static constexpr uint32_t ParticleImage = 8;

    static constexpr uint32_t MaterialColorMap = 10;
    static constexpr uint32_t MaterialMetallicRoughnessMap = 11;
    static constexpr uint32_t MaterialNormalMap = 12;
    static constexpr uint32_t MaterialEmissionMap = 13;

    static constexpr uint32_t ReservedEnd = 15;
};
