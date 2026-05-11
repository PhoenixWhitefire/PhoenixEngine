// Environment.cpp, World ambience and lighting - 22/03/2026
#include <filesystem>
#include <tracy/Tracy.hpp>
#include <glad/gl.h>

#include "component/Environment.hpp"
#include "asset/TextureManager.hpp"
#include "render/TextureSlots.hpp"
#include "Utilities.hpp"
#include "Engine.hpp"
#include "FileRW.hpp"

static const std::string_view SkyboxFaces[6] = {
    "right",
	"left",
	"top",
	"bottom",
	"front",
	"back"
};

static std::unordered_map<std::string, uint32_t> CachedSkyboxCubemaps;

static uint32_t startLoadingSkyboxCubemap(EcEnvironmentService* env)
{
	ZoneScoped;

	if (Engine::Get()->IsHeadlessMode)
		return 0;

    if (const auto& it = CachedSkyboxCubemaps.find(env->Skybox); it != CachedSkyboxCubemaps.end())
        return it->second;

    GLuint skyboxCubemap = 0;
	glGenTextures(1, &skyboxCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    CachedSkyboxCubemaps[env->Skybox] = skyboxCubemap;

	for (uint8_t faceIndex = 0; faceIndex < 6; faceIndex++)
	{
		const std::string_view& face = SkyboxFaces[faceIndex];

		uint32_t tex = TextureManager::Get()->LoadFromPath(std::format("{}/{}.jpg", env->Skybox, face));
		env->SkyboxFacesBeingLoaded.push_back(tex);

		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
			0,
			GL_SRGB,
			1,
			1,
			0,
			GL_RED,
			GL_UNSIGNED_BYTE,
			TextureManager::Get()->GetTextureResource(tex).TMP_ImageByteData
		);
	}

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	return skyboxCubemap;
}

void EcEnvironmentService::ChangeSkybox(const std::string_view& pathStr)
{
    std::filesystem::path path = FileRW::ResolvePathNormalized(std::string(pathStr));
    if (pathStr[0] != '!' && std::filesystem::is_directory(path))
    {
        // Check mirrors the one in `TextureManager::m_UploadTextureToGpu`
        // It intentionally does not free the texture to allow the Engine to put
        // it into the cubemap in the main loop
        if (pathStr.find("Sky") == std::string::npos && pathStr[0] != '!')
            RAISE_RT("Invalid skybox path '{}': Cubemap directory paths must have 'Sky' (case-sensitive) somewhere in it :)", pathStr);

        for (const std::string_view& face : SkyboxFaces)
        {
            std::string facePath = (path / face).string() + ".jpg";
            if (!std::filesystem::is_regular_file(facePath))
                RAISE_RT("Invalid skybox path '{}': Is a directory, but does not have file for the {} face (expected at {})", path.string(), face, facePath);
        }

        SkyboxIsEquirectangularImage = false;
        Skybox = pathStr;
        SkyboxTextureGpuId = startLoadingSkyboxCubemap(this);
    }
    else if (pathStr[0] == '!' || std::filesystem::is_regular_file(path))
    {
        SkyboxIsEquirectangularImage = true;
        EcEnvironmentService::Skybox = pathStr;

        TextureManager* texManager = TextureManager::Get();
        uint32_t textureResourceId = texManager->LoadFromPath(path.string());
        SkyboxTextureGpuId = texManager->GetTextureResource(textureResourceId).GpuId;

        glActiveTexture(GL_TEXTURE0 + ReservedTextureSlot::SkyboxEquirectangular);
        glBindTexture(GL_TEXTURE_2D, SkyboxTextureGpuId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else
        RAISE_RT("Invalid skybox path '{}': Expected file or directory", path.string());
}

static EcEnvironmentService DefaultInstance;

void EnvironmentComponentManager::BindService(uint32_t Id)
{
    EcEnvironmentService* env = (EcEnvironmentService*)GetComponent(Id);
    env->ChangeSkybox(env->Skybox);
    ServiceInstance = env->Object;
}

void EnvironmentComponentManager::UnbindService()
{
    ServiceInstance.Clear();
}

EcEnvironmentService* EnvironmentComponentManager::GetService() const
{
    if (ServiceInstance && ServiceInstance->FindComponent<EcEnvironmentService>())
        return ServiceInstance->FindComponent<EcEnvironmentService>();
    else
        return &DefaultInstance;
}

uint32_t EnvironmentComponentManager::CreateComponent(GameObject* Object)
{
    uint32_t id = ComponentManager<EcEnvironmentService>::CreateComponent(Object);
    m_Components[id].Object = Object;

    return id;
}

const Reflection::StaticPropertyMap& EnvironmentComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY_SIMPLE_NGV(EcEnvironmentService, AmbientLight, Color),
        REFLECTION_PROPERTY_SIMPLE(EcEnvironmentService, Fog, Boolean),
        REFLECTION_PROPERTY_SIMPLE_NGV(EcEnvironmentService, FogColor, Color),
        REFLECTION_PROPERTY_SIMPLE(EcEnvironmentService, PostProcess, Boolean),
        REFLECTION_PROPERTY_SIMPLE(EcEnvironmentService, GammaCorrection, Double),

        REFLECTION_PROPERTY(
            "Skybox",
            String,
            [](void* p) -> Reflection::GenericValue
            {
                return static_cast<EcEnvironmentService*>(p)->Skybox;
            },
            [](void* p, const Reflection::GenericValue& gv)
            {
                static_cast<EcEnvironmentService*>(p)->ChangeSkybox(gv.AsStringView());
            }
        )
    };

    return props;
}
