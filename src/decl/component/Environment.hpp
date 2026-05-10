// Environment.cpp, World ambience and lighting - 22/03/2026
#pragma once

#include "datatype/ComponentBase.hpp"
#include "datatype/Color.hpp"

struct EcEnvironmentService : public Component<EntityComponent::Environment>
{
    EcEnvironmentService();

    static void ChangeSkybox(const std::string_view& Path);

    static inline Color AmbientLight = { .3f, .3f, .3f };
    static inline bool PostProcess = false;
    static inline bool Fog = false;
    static inline Color FogColor = { 0.85f, 0.85f, 0.90f };
    static inline float GammaCorrection = 1.f;
    static inline std::string Skybox = "textures/Sky1";
    static inline uint32_t SkyboxTextureGpuId = UINT32_MAX;
    static inline bool SkyboxIsEquirectangularImage = true;
    static inline std::vector<uint32_t> SkyboxFacesBeingLoaded;

    bool Valid = true;
};

class EnvironmentComponentManager : public ComponentManager<EcEnvironmentService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
};
