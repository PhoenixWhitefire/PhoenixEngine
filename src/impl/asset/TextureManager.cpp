#include <format>
#include <functional>
#include <glad/gl.h>
#include <stb/stb_image.h>

#include "asset/TextureManager.hpp"
#include "render/ShaderProgram.hpp"
#include "GlobalJsonConfig.hpp"
#include "ThreadManager.hpp"
#include "Debug.hpp"

static const std::string MissingTexPath = "textures/missing.png";

typedef std::function<uint8_t* (const char*, int*, int*, int*)> ImageLoader_t;
typedef std::function<Texture*(ImageLoader_t, Texture*, std::string, uint32_t)> AsyncTexLoader_t;

// a 2x2 purple-and-black checkerboard
// 23/10/2024
static uint8_t MissingTextureBytes[12] = 
{
	static_cast<uint8_t>(0xFFFFFFu),
	static_cast<uint8_t>(0x000000u),
	static_cast<uint8_t>(0xFFFFFFu),

	static_cast<uint8_t>(0x000000u),
	static_cast<uint8_t>(0x000000u),
	static_cast<uint8_t>(0x000000u),

	static_cast<uint8_t>(0x000000u),
	static_cast<uint8_t>(0x000000u),
	static_cast<uint8_t>(0x000000u),

	static_cast<uint8_t>(0xFFFFFFu),
	static_cast<uint8_t>(0x000000u),
	static_cast<uint8_t>(0xFFFFFFu),
};

void TextureManager::m_UploadTextureToGpu(Texture& texture)
{
	if (texture.Status == TextureLoadStatus::Failed)
	{
		std::string fallbackPath = MissingTexPath;

		if (texture.ImagePath != MissingTexPath)
		{
			auto FormattedArgs = std::make_format_args(texture.ImagePath);
			Debug::Log(std::vformat("Failed to load texture '{}'", FormattedArgs));
		}
		else
			fallbackPath = "!Missing";

		uint32_t replacementId = this->LoadTextureFromPath(
			fallbackPath,
			false
		);

		Texture* replacement = this->GetTextureResource(replacementId);
		texture.Height = replacement->Height;
		texture.Width = replacement->Width;
		texture.NumColorChannels = replacement->NumColorChannels;
		texture.TMP_ImageByteData = replacement->TMP_ImageByteData;
		texture.GpuId = replacement->GpuId;

		return;
	}

	GLenum Format = 0;

	switch (texture.NumColorChannels)
	{

	case (4):
	{
		Format = GL_RGBA;
		break;
	}

	case (3):
	{
		Format = GL_RGB;
		break;
	}

	case (1):
	{
		Format = GL_RED;
		break;
	}

	default:
	{
		throw(std::vformat(
			std::string("Invalid ImageNumColorChannels (was '{}') for '{}'!"),
			std::make_format_args(texture.NumColorChannels, texture.ImagePath)
		));
		break;
	}

	};

	//if ((Format == GL_RGB) && TexturePtr->Usage == TextureType::SPECULAR)
	//	Format = GL_RED;

	// TODO: texture mipmaps, 4 is the number of mipmaps
	//glTexStorage2D(GL_TEXTURE_2D, 1, MipMapsInternalFormat, TexturePtr->ImageWidth, TexturePtr->ImageHeight);

	glBindTexture(GL_TEXTURE_2D, texture.GpuId);

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

	glGenerateMipmap(GL_TEXTURE_2D);

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

TextureManager::TextureManager()
	: m_Textures{}, m_TexPromises{}
{
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
	missingTexture.Status = TextureLoadStatus::Succeeded;

	glBindTexture(GL_TEXTURE_2D, missingTexture.GpuId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	m_UploadTextureToGpu(missingTexture);

	// dev-specified missing texture is id 2
	this->LoadTextureFromPath(MissingTexPath, false);
}

TextureManager::~TextureManager()
{
	std::vector<uint32_t> textureGpuIds;
	textureGpuIds.reserve(m_Textures.size());

	for (const Texture& texture : m_Textures)
		textureGpuIds.push_back(texture.GpuId);
	
	glDeleteTextures(static_cast<int32_t>(m_Textures.size()), textureGpuIds.data());

	for (size_t promIndex = 0; promIndex < m_TexPromises.size(); promIndex++)
	{
		std::promise<Texture>* prom = m_TexPromises[promIndex];
		std::shared_future<Texture>& future = m_TexFutures[promIndex];

		// we don't want our async threads accessing deleted promises
		future.wait();

		delete prom;
	}

	m_Textures.clear();
	m_StringToTextureId.clear();
	m_TexFutures.clear();
	m_TexPromises.clear();
}

static TextureManager* instance = nullptr;

TextureManager* TextureManager::Get()
{
	if (!instance)
		instance = new TextureManager();
	return instance;
}

void TextureManager::Shutdown()
{
	delete instance;
	instance = nullptr;
}

static uint8_t* loadImageData(const char* ImagePath, int* ImageWidth, int* ImageHeight, int* ImageColorChannels)
{
	stbi_set_flip_vertically_on_load(true);

	uint8_t* imageData = stbi_load(ImagePath, ImageWidth, ImageHeight, ImageColorChannels, 0);
	
	return imageData;
}

static void asyncTextureLoader(
	ImageLoader_t ImageLoader,
	Texture* AsyncTexture,
	std::string ActualPath
)
{
	uint8_t* data = ImageLoader(
		ActualPath.c_str(),
		&AsyncTexture->Width,
		&AsyncTexture->Height,
		&AsyncTexture->NumColorChannels
	);

	AsyncTexture->Status = data ? TextureLoadStatus::Succeeded : TextureLoadStatus::Failed;

	AsyncTexture->TMP_ImageByteData = data;
}

uint32_t TextureManager::LoadTextureFromPath(const std::string& Path, bool ShouldLoadAsync)
{
	std::string ResDir = EngineJsonConfig["ResourcesDirectory"];
	std::string ActualPath = ResDir + Path;

	auto it = m_StringToTextureId.find(Path);
	
	if (it == m_StringToTextureId.end())
	{
		uint32_t newResourceId = static_cast<uint32_t>(m_Textures.size());

		m_StringToTextureId.insert(std::pair(Path, newResourceId));

		uint32_t newGpuId;
		glGenTextures(1, &newGpuId);

		m_Textures.emplace_back(Path, newResourceId, newGpuId);

		Texture& newTexture = m_Textures.at(newResourceId);

		glBindTexture(GL_TEXTURE_2D, newTexture.GpuId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		static const uint8_t FullByte = 0xFF;

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
			&FullByte
		);

		glGenerateMipmap(GL_TEXTURE_2D);

		//ShouldLoadAsync = false;

		if (ShouldLoadAsync)
		{
			/*Task* LoadTextureTask = new Task();
			LoadTextureTask->FuncArgument = (void*)this;
			LoadTextureTask->Function = asyncTextureLoader;
			LoadTextureTask->DbgInfo = "LoadTextureTask: Tex: " + ActualPath;

			ThreadManager::Get()->DispatchJob(*LoadTextureTask);*/

			std::promise<Texture>* promise = new std::promise<Texture>;

			std::thread(
				[promise, ActualPath, newResourceId](auto asyncFunc, auto imageLoader)
				{
					Texture asyncTexture{};
					asyncTexture.ResourceId = newResourceId;

					asyncFunc(imageLoader, &asyncTexture, ActualPath);

					promise->set_value_at_thread_exit(asyncTexture);
				},
				asyncTextureLoader,
				loadImageData
			).detach();

			newTexture.Status = TextureLoadStatus::InProgress;

			m_TexPromises.push_back(promise);
			m_TexFutures.push_back(promise->get_future().share());
		}
		else
		{
			uint8_t* data = loadImageData(
				ActualPath.c_str(),
				&newTexture.Width,
				&newTexture.Height,
				&newTexture.NumColorChannels
			);

			newTexture.TMP_ImageByteData = data;
			newTexture.Status = data ? TextureLoadStatus::Succeeded : TextureLoadStatus::Failed;

			m_UploadTextureToGpu(newTexture);
		}

		return newResourceId;
	}
	else
		return it->second;
}

Texture* TextureManager::GetTextureResource(uint32_t ResourceId)
{
	return &m_Textures.at(ResourceId);
}

void TextureManager::FinalizeAsyncLoadedTextures()
{
	size_t numTexPromises = m_TexPromises.size();
	size_t numTexFutures = m_TexFutures.size();

	if (numTexPromises != numTexFutures)
	{
		Debug::Log(std::vformat(
			"FinalizeAsyncLoadedTextures had {} promises but {} futures, cannot proceed safely",
			std::make_format_args(numTexPromises, numTexFutures)
		));
		return;
	}

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

		if (image.Status != TextureLoadStatus::InProgress)
			continue;

		image.Status = loadedImage.Status;

		if (image.Status == TextureLoadStatus::Succeeded)
		{
			image.Width = loadedImage.Width;
			image.Height = loadedImage.Height;
			image.NumColorChannels = loadedImage.NumColorChannels;

			size_t bufSize = (size_t)image.Width * (size_t)image.Height * (size_t)image.NumColorChannels;

			image.TMP_ImageByteData = (uint8_t*)malloc(bufSize);

			if (!image.TMP_ImageByteData)
				throw(std::vformat(
					"`malloc` failed in ::FinalizeAsyncLoadedTextures (Requested amount was {} bytes)",
					std::make_format_args(bufSize)
				));

			// if image fails to load this will be NULL
			if (loadedImage.TMP_ImageByteData)
				memcpy(image.TMP_ImageByteData, loadedImage.TMP_ImageByteData, bufSize);

			stbi_image_free(loadedImage.TMP_ImageByteData);
		}

		m_UploadTextureToGpu(image);
		
		m_TexPromises.erase(m_TexPromises.begin() + promiseIndex);
		m_TexFutures.erase(m_TexFutures.begin() + promiseIndex);

		delete promise;

		return;
	}
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