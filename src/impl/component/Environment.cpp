// Environment.cpp, World ambience and lighting - 22/03/2026
#include <filesystem>
#include <tracy/Tracy.hpp>
#include <glad/gl.h>

#include "component/Environment.hpp"
#include "asset/TextureManager.hpp"
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

static uint32_t startLoadingSkyboxCubemap()
{
	ZoneScoped;

	if (Engine::Get()->IsHeadlessMode)
		return 0;

	GLuint skyboxCubemap = 0;
	glGenTextures(1, &skyboxCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxCubemap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	for (uint8_t faceIndex = 0; faceIndex < 6; faceIndex++)
	{
		const std::string_view& face = SkyboxFaces[faceIndex];

		uint32_t tex = TextureManager::Get()->LoadFromPath(std::format("{}/{}.jpg", EcEnvironmentService::Skybox, face));
		EcEnvironmentService::SkyboxFacesBeingLoaded.push_back(tex);

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
    bool isEquirect = false;

    std::filesystem::path path = FileRW::ResolvePathNormalized(std::string(pathStr));
    if (pathStr[0] != '!' && std::filesystem::is_directory(path))
    {
        for (const std::string_view& face : SkyboxFaces)
        {
            if (!std::filesystem::is_regular_file((path / face).string() + ".jpg"))
                RAISE_RT("Invalid skybox path '{}': Is a directory, but does not have file for the {} face (expected at {})", path.string(), face, (path / face / ".jpg").string());
        }
        isEquirect = false;
        EcEnvironmentService::Skybox = path;
        EcEnvironmentService::SkyboxTextureGpuId = startLoadingSkyboxCubemap();
    }
    else if (pathStr[0] == '!' || std::filesystem::is_regular_file(path))
    {
        isEquirect = true;
        EcEnvironmentService::Skybox = path;

        TextureManager* texManager = TextureManager::Get();
        uint32_t textureResourceId = texManager->LoadFromPath(path.string());
        EcEnvironmentService::SkyboxTextureGpuId = texManager->GetTextureResource(textureResourceId).GpuId;
    }
    else
        RAISE_RT("Invalid skybox path '{}': Expected file or directory", path.string());

    EcEnvironmentService::SkyboxIsEquirectangularImage = isEquirect;
}

EcEnvironmentService::EcEnvironmentService()
{
    static bool DidInit = false;
    if (DidInit)
        return;
    DidInit = true;

    EcEnvironmentService::SkyboxTextureGpuId = TextureManager::Get()->LoadFromPath("!White");
    EcEnvironmentService::SkyboxIsEquirectangularImage = true;
    EcEnvironmentService::SkyboxFacesBeingLoaded.reserve(6);
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
            [](void*) -> Reflection::GenericValue
            {
                return EcEnvironmentService::Skybox;
            },
            [](void*, const Reflection::GenericValue& gv)
            {
                EcEnvironmentService::ChangeSkybox(gv.AsStringView());
            }
        )
    };

    return props;
}
