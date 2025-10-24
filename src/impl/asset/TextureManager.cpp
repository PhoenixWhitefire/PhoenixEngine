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

void TextureManager::Initialize(bool IsHeadless)
{
	ZoneScoped;

	m_IsHeadless = IsHeadless;

	if (IsHeadless)
	{
		s_Instance = this;
		return;
	}

	// ID 0 means no texture
	m_Textures.emplace_back();

	// checkboard missing texture is id 1
	m_StringToTextureId.insert(std::pair("!Missing", 1));

	uint32_t newGpuId;
	glGenTextures(1, &newGpuId);

	m_Textures.emplace_back("!Missing", 1, newGpuId);

	Texture& missingTexture = m_Textures.at(1);
	missingTexture.Width = 2;
	missingTexture.Height = 2;
	missingTexture.NumColorChannels = 3;
	missingTexture.TMP_ImageByteData = MissingTextureBytes;
	missingTexture.Status = Texture::LoadStatus::Succeeded;

	glBindTexture(GL_TEXTURE_2D, missingTexture.GpuId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	m_UploadTextureToGpu(missingTexture);

	s_Instance = this;
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
	
	if (it != m_StringToTextureId.end())
	{
		const Texture& texture = m_Textures.at(it->second);

		if (texture.Status != Texture::LoadStatus::Unloaded)
		{
			if (texture.DoBilinearSmoothing != DoBilinearSmoothing)
			{
				for (const Texture& other : m_Textures)
				{
					if (other.ImagePath == ActualPath && other.DoBilinearSmoothing == DoBilinearSmoothing)
						return other.ResourceId;
				}

				it = m_StringToTextureId.end();
			}
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
				newResourceId = this->Assign({ .ImagePath = ActualPath, .ResourceId = UINT32_MAX }, ActualPath);
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

		f.wait();

		const Texture& loadedImage = f.get();

		Texture& image = m_Textures.at(loadedImage.ResourceId);

		if (image.Status != Texture::LoadStatus::InProgress)
			continue;

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