// Environment.cpp, World ambience and lighting - 22/03/2026
#pragma once

#include "datatype/ComponentBase.hpp"
#include "datatype/Color.hpp"

struct EcEnvironmentService : public Component<EntityComponent::Environment>
{
    static inline Color AmbientLight = { .3f, .3f, .3f };
    static inline bool PostProcess = false;
    static inline bool Fog = false;
    static inline Color FogColor = { 0.85f, 0.85f, 0.90f };
    static inline float GammaCorrection = 1.f;

    bool Valid = true;
};
