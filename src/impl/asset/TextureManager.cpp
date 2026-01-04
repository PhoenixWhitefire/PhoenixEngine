#include <format>
#include <mutex>
#include <functional>
#include <glad/gl.h>
#include <stb/stb_image.h>
#include <tracy/Tracy.hpp>

#include "asset/TextureManager.hpp"
#include "GlobalJsonConfig.hpp"
#include "ThreadManager.hpp"
#include "Utilities.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

static const std::string MissingTexPath = "!Missing";

typedef std::function<uint8_t* (const char*, int*, int*, int*)> ImageLoader_t;
typedef std::function<Texture*(ImageLoader_t, Texture*, std::string, uint32_t)> AsyncTexLoader_t;

// a 2x2 purple-and-black checkerboard
// 23/10/2024
static uint8_t MissingTextureBytes[] = 
{
	static_cast<uint8_t>(0xFFu),
	static_cast<uint8_t>(0x00u),
	static_cast<uint8_t>(0xFFu),

	static_cast<uint8_t>(0x00u),
	static_cast<uint8_t>(0x00u),
	static_cast<uint8_t>(0x00u),

	static_cast<uint8_t>(0x00u),
	static_cast<uint8_t>(0x00u),
	static_cast<uint8_t>(0x00u),

	static_cast<uint8_t>(0xFFu),
	static_cast<uint8_t>(0x00u),
	static_cast<uint8_t>(0xFFu),
};

static uint32_t WhiteTextureBytes = 0xFFFFFF;
static uint32_t BlackTextureBytes = 0x000000;

static bool s_TextureManagerShutdown = false;

void TextureManager::m_UploadTextureToGpu(Texture& texture)
{
	ZoneScoped;
	assert(!m_IsHeadless);

	if (texture.Status == Texture::LoadStatus::Failed)
	{
		std::string fallbackPath = MissingTexPath;

		if (texture.ImagePath != MissingTexPath)
			Log::ErrorF(
				"Failed to load texture '{}': {}",
				texture.ImagePath, texture.FailureReason
			);
		else
			fallbackPath = "!Missing";

		uint32_t texId = texture.ResourceId;
		uint32_t replacementId = this->LoadTextureFromPath(
			fallbackPath,
			false
		);

		texture = GetTextureResource(texId); // in case `m_Textures` got re-alloc'd
		glDeleteTextures(1, &texture.GpuId);

		Texture& replacement = this->GetTextureResource(replacementId);
		texture.Height = replacement.Height;
		texture.Width = replacement.Width;
		texture.NumColorChannels = replacement.NumColorChannels;
		texture.TMP_ImageByteData = replacement.TMP_ImageByteData;
		texture.GpuId = replacement.GpuId;

		return;
	}

	GLenum Format = 0;
	const GLenum NumChannelsToFormat[] = { 0, GL_RED, GL_RG, GL_RGB, GL_RGBA };

	if (texture.NumColorChannels < 1 || texture.NumColorChannels > 4)
	{
		Log::ErrorF(
			"Unsupported number of color channels '{}' for texture '{}'",
			texture.NumColorChannels, texture.ImagePath
		);
		return;
	}
	else
		Format = NumChannelsToFormat[texture.NumColorChannels];

	glBindTexture(GL_TEXTURE_2D, texture.GpuId);

	// "cupid-ref.jpg" some weird bullshit, doesn't happen when it's
	// saved as PNG, but persists after re-saving as JPG
	// what the what
	// also fixes the Missing Texture?? damn khr what the fuck
	// 30/11/2024
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	{
		ZoneScopedN("UploadData");

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA,
			texture.Width,
			texture.Height,
			0,
			Format,
			GL_UNSIGNED_BYTE,
			texture.TMP_ImageByteData
		);
	}

	{
		ZoneScopedN("GenerateMipMaps");
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	// Can't free this now bcuz Engine.cpp needs it for skybox images
	// 23/08/2024
	// 03/10/2024:
	// little hack to `free` textures `Engine.cpp` doesn't
	// manage (most of them)
	if (texture.ImagePath.find("Sky") == std::string::npos && texture.ImagePath != "!Missing")
	{
		free(texture.TMP_ImageByteData);
		texture.TMP_ImageByteData = nullptr;
	}

	glBindTexture(GL_TEXTURE_2D, 0);
}

static TextureManager* s_Instance = nullptr;

static void createAndUploadTextureData(const std::string& Name, uint8_t* Data, int Width, int Height)
{
	s_Instance->m_StringToTextureId.insert(std::pair(Name, (uint32_t)s_Instance->m_Textures.size()));

	uint32_t newGpuId;
	glGenTextures(1, &newGpuId);

	s_Instance->m_Textures.emplace_back("!Missing", (uint32_t)s_Instance->m_Textures.size(), newGpuId);

	Texture& tex = s_Instance->m_Textures.back();
	tex.Width = Width;
	tex.Height = Height;
	tex.NumColorChannels = 3;
	tex.TMP_ImageByteData = Data;
	tex.Status = Texture::LoadStatus::Succeeded;

	glBindTexture(GL_TEXTURE_2D, tex.GpuId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	s_Instance->m_UploadTextureToGpu(tex);
}

void TextureManager::Initialize(bool IsHeadless)
{
	ZoneScoped;

	m_IsHeadless = IsHeadless;
	s_Instance = this;

	if (IsHeadless)
		return;

	// ID 0 means no texture
	m_Textures.emplace_back();

	createAndUploadTextureData("!Missing", MissingTextureBytes, 2, 2);
	createAndUploadTextureData("!White", (uint8_t*)&WhiteTextureBytes, 1, 1);
	createAndUploadTextureData("!Black", (uint8_t*)&BlackTextureBytes, 1, 1);
}

void TextureManager::Shutdown()
{
	if (m_IsHeadless)
	{
		s_Instance = nullptr;
		return;
	}

	s_TextureManagerShutdown = true;

	std::vector<uint32_t> textureGpuIds;
	textureGpuIds.reserve(m_Textures.size());

	for (const Texture& texture : m_Textures)
		textureGpuIds.push_back(texture.GpuId);

	glDeleteTextures(static_cast<int32_t>(m_Textures.size()), textureGpuIds.data());

	m_Textures.clear();
	m_StringToTextureId.clear();
	m_TexFutures.clear();
	m_TexPromises.clear();

	s_Instance = nullptr;
}

TextureManager::~TextureManager()
{
	assert(!s_Instance);
}

TextureManager* TextureManager::Get()
{
	assert(s_Instance);
	return s_Instance;
}

// like emplace, for "put in place", but "emload" for "load in place"
// i think that's how english works maybe
// 05/12/2024
static void emloadTexture(
	Texture* AsyncTexture,
	std::string ActualPath
)
{
	ZoneScoped;

	AsyncTexture->NumColorChannels = 4;

	uint8_t* data = nullptr;
	
	{
		ZoneScopedN("stbi_load");

		data = stbi_load(
			ActualPath.c_str(),
			&AsyncTexture->Width,
			&AsyncTexture->Height,
			&AsyncTexture->NumColorChannels,
			0
		);
	}
	
	AsyncTexture->Status = data ? Texture::LoadStatus::Succeeded : Texture::LoadStatus::Failed;
	AsyncTexture->TMP_ImageByteData = data;

	if (!data)
		AsyncTexture->FailureReason = stbi_failure_reason();
}

uint32_t TextureManager::Assign(const Texture& texture, const std::string& name)
{
	if (texture.TMP_ImageByteData != nullptr)
		Log::WarningF(
			"The Texture being assigned to '{}' has non-NULL byte data and may create UB",
			name
		);

	if (texture.ResourceId != UINT32_MAX)
	{
		m_Textures.at(texture.ResourceId) = texture;
		m_StringToTextureId[name] = texture.ResourceId;
	}

	uint32_t assignedId = static_cast<uint32_t>(m_Textures.size());

	auto prevPair = m_StringToTextureId.find(name);

	if (prevPair != m_StringToTextureId.end())
	{
		// overwrite the pre-existing texture
		m_Textures[prevPair->second] = texture;
		assignedId = prevPair->second;
	}
	else
	{
		m_StringToTextureId.insert(std::pair(name, assignedId));
		m_Textures.push_back(texture);
	}

	m_Textures[assignedId].ResourceId = assignedId;

	return assignedId;
}

uint32_t TextureManager::LoadTextureFromPath(const std::string& Path, bool ShouldLoadAsync, bool DoBilinearSmoothing)
{
	if (m_IsHeadless)
		return 0;

	std::string ResDir = EngineJsonConfig["ResourcesDirectory"];
	std::string ActualPath = FileRW::MakePathCwdRelative(Path);

	auto it = m_StringToTextureId.find(ActualPath);
	uint32_t forceResourceId = UINT32_MAX;
	std::string assignName = ActualPath;
	
	if (it != m_StringToTextureId.end())
	{
		const Texture& texture = m_Textures.at(it->second);

		if (texture.Status != Texture::LoadStatus::Unloaded)
		{
			if (texture.DoBilinearSmoothing != DoBilinearSmoothing)
			{
				assignName = ActualPath + (DoBilinearSmoothing ? "!B" : "!N");
				const auto& vit = m_StringToTextureId.find(assignName);

				if (vit != m_StringToTextureId.end())
					return vit->second;

				it = m_StringToTextureId.end();
			}
			else
				return it->second;
		}
		else
		{
			forceResourceId = it->second;
		}
	}

	if (it == m_StringToTextureId.end() || forceResourceId != UINT32_MAX)
	{
		uint32_t newGpuId;
		uint32_t newResourceId = UINT32_MAX;
		Texture* newTexture = nullptr;

		{
			ZoneScopedN("CreateResource");

			glGenTextures(1, &newGpuId);

			if (forceResourceId == UINT32_MAX)
				newResourceId = this->Assign({ .ImagePath = ActualPath, .ResourceId = UINT32_MAX }, assignName);
			else
				newResourceId = forceResourceId;

			newTexture = &this->GetTextureResource(newResourceId);
			newTexture->DoBilinearSmoothing = DoBilinearSmoothing;
			newTexture->GpuId = newGpuId;

			glBindTexture(GL_TEXTURE_2D, newTexture->GpuId);

			if (DoBilinearSmoothing)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}

		//ShouldLoadAsync = false;

		if (ShouldLoadAsync)
		{
			ZoneScopedN("CreateAsyncJob");

			static const uint32_t BlackPixel = 0u;

			glTexImage2D
			(
				GL_TEXTURE_2D,
				0,
				GL_RGBA,
				1,
				1,
				0,
				GL_RED,
				GL_UNSIGNED_BYTE,
				&BlackPixel
			);

			std::promise<Texture>* promise = new std::promise<Texture>;

			ThreadManager::Get()->Dispatch(
				[promise, ActualPath, newResourceId]()
				{
					ZoneScopedN("AsyncTextureLoad");
					ZoneText(ActualPath.data(), ActualPath.size());

					Texture asyncTexture{};
					asyncTexture.LoadedAsynchronously = true;
					asyncTexture.ResourceId = newResourceId;

					emloadTexture(&asyncTexture, ActualPath);

					promise->set_value(asyncTexture);
				},
				false
			);

			newTexture->Status = Texture::LoadStatus::InProgress;

			m_TexPromises.push_back(promise);
			m_TexFutures.push_back(promise->get_future().share());
		}
		else
		{
			ZoneScopedN("LoadSynchronous");

			emloadTexture(newTexture, ActualPath);
			m_UploadTextureToGpu(*newTexture);
		}

		return newResourceId;
	}
	else
		return it->second;
}

Texture& TextureManager::GetTextureResource(uint32_t ResourceId)
{
	if (ResourceId < m_Textures.size())
		return m_Textures.at(ResourceId);
	else
		return m_Textures[1];
}

void TextureManager::FinalizeAsyncLoadedTextures()
{
	ZoneScoped;

	if (m_IsHeadless)
		return;

	size_t numTexPromises = m_TexPromises.size();

	for (size_t promiseIndex = 0; promiseIndex < numTexPromises; promiseIndex++)
	{
		std::promise<Texture>* promise = m_TexPromises[promiseIndex];
		std::shared_future<Texture>& f = m_TexFutures[promiseIndex];

		// https://stackoverflow.com/a/10917945/16875161
		// TODO 10/09/2024
		// Surely there's gotta be a better way than this `wait_for` bullshit
		if (!f.valid() || (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready))
			continue;

		const Texture& loadedImage = f.get();
		Texture& image = m_Textures.at(loadedImage.ResourceId);

		ZoneScopedN("TextureReady");
		ZoneText(image.ImagePath.data(), image.ImagePath.size());

		if (image.Status != Texture::LoadStatus::InProgress)
		{
			assert(false); // TODO what is this condition
			continue;
		}

		image.Status = loadedImage.Status;
		image.Width = loadedImage.Width;
		image.Height = loadedImage.Height;
		image.NumColorChannels = loadedImage.NumColorChannels;
		image.FailureReason = loadedImage.FailureReason;
		image.LoadedAsynchronously = loadedImage.LoadedAsynchronously;

		if (image.Status == Texture::LoadStatus::Succeeded)
		{
			size_t bufSize = (size_t)image.Width * (size_t)image.Height * (size_t)image.NumColorChannels;

			image.TMP_ImageByteData = (uint8_t*)malloc(bufSize);

			PHX_ENSURE_MSG(image.TMP_ImageByteData, std::format(
				"`malloc` failed in ::FinalizeAsyncLoadedTextures (Requested amount was {} bytes)",
				bufSize
			));

			// if image fails to load this will be NULL
			if (loadedImage.TMP_ImageByteData)
				memcpy(image.TMP_ImageByteData, loadedImage.TMP_ImageByteData, bufSize);

			free(loadedImage.TMP_ImageByteData);
		}

		m_UploadTextureToGpu(image);
		
		m_TexPromises.erase(m_TexPromises.begin() + promiseIndex);
		m_TexFutures.erase(m_TexFutures.begin() + promiseIndex);

		delete promise;

		numTexPromises = m_TexPromises.size();

		if (numTexPromises != m_TexPromises.size())
		{
			promiseIndex--;
			numTexPromises = m_TexPromises.size();
		}
	}
}

void TextureManager::UnloadTexture(uint32_t Id)
{
	Texture& tex = GetTextureResource(Id);
	if (tex.Status == Texture::LoadStatus::Unloaded)
		return;

	m_StringToTextureId.erase(tex.ImagePath);

	if (tex.GpuId > 0 && tex.GpuId != UINT32_MAX)
		glDeleteTextures(1, &tex.GpuId);
	tex.GpuId = GetTextureResource(1).GpuId;

	tex.Status = Texture::LoadStatus::Unloaded;
}

/*
void Texture::Bind() {
	glActiveTexture(GL_TEXTURE0 + this->Slot);
	glBindTexture(GL_TEXTURE_2D, this->ID);
}

void Texture::Unbind() {
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Destroy() {
	glDeleteTextures(1, &this->ID);
}
*/