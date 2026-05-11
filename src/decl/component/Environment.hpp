// Environment.cpp, World ambience and lighting - 22/03/2026
#pragma once

#include "datatype/ComponentBase.hpp"
#include "datatype/Color.hpp"

struct EcEnvironmentService : public Component<EntityComponent::Environment>
{
    void ChangeSkybox(const std::string_view& Path);

    ObjectRef Object;

    Color AmbientLight = { .3f, .3f, .3f };
    Color FogColor = { 0.85f, 0.85f, 0.90f };
    float GammaCorrection = 1.f;

    std::vector<uint32_t> SkyboxFacesBeingLoaded;
    std::string Skybox = "textures/Sky1";
    uint32_t SkyboxTextureGpuId = UINT32_MAX;
    bool SkyboxIsEquirectangularImage = true;

    bool PostProcess = false;
    bool Fog = false;

    bool Valid = true;
};

class EnvironmentComponentManager : public ComponentManager<EcEnvironmentService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
    uint32_t CreateComponent(GameObject*) override;

    void BindService(uint32_t) override;
    void UnbindService() override;

    EcEnvironmentService* GetService() const;
    ObjectHandle ServiceInstance;
};
