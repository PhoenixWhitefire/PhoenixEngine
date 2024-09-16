#include<format>
#include<functional>
#include<glad/gl.h>
#include<stb/stb_image.h>

#include"asset/TextureManager.hpp"
#include"render/ShaderProgram.hpp"
#include"GlobalJsonConfig.hpp"
#include"ThreadManager.hpp"
#include"Debug.hpp"

static const std::string MissingTexPath = "textures/MISSING2_MaximumADHD_status_1665776378145304579.png";
static const std::string TexLoadErr =
"Attempted to load texture '{}', but it failed. Additionally, the replacement Missing Texture could not be loaded.";

typedef std::function<uint8_t* (const char*, int*, int*, int*)> ImageLoader_t;
typedef std::function<Texture*(ImageLoader_t, Texture*, std::string, uint32_t)> AsyncTexLoader_t;

static void registerTexture(Texture& texture)
{
	if (texture.Status == TextureLoadStatus::Failed)
	{
		auto FormattedArgs = std::make_format_args(texture.ImagePath);
		Debug::Log(std::vformat("Failed to load texture '{}'", FormattedArgs));

		if (texture.ImagePath != MissingTexPath)
		{
			TextureManager* texManager = TextureManager::Get();
			uint32_t replacementId = texManager->LoadTextureFromPath(MissingTexPath, false);

			Texture* replacement = texManager->GetTextureResource(replacementId);
			texture.Height = replacement->Height;
			texture.Width = replacement->Width;
			texture.NumColorChannels = replacement->NumColorChannels;
			texture.TMP_ImageByteData = replacement->TMP_ImageByteData;

			glBindTexture(GL_TEXTURE_2D, texture.GpuId);

			glTexImage2D
			(
				GL_TEXTURE_2D,
				0,
				GL_RGBA,
				texture.Width,
				texture.Height,
				0,
				replacement->NumColorChannels == 1 ? GL_RED
					: (replacement->NumColorChannels == 3 ? GL_RGB : GL_RGBA),
				GL_UNSIGNED_BYTE,
				texture.TMP_ImageByteData
			);

			glGenerateMipmap(GL_TEXTURE_2D);

			glBindTexture(GL_TEXTURE_2D, 0);

			return;
		}
		else
			throw(std::vformat(TexLoadErr, std::make_format_args(texture.ImagePath)));
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

	glTexImage2D
	(
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
	//stbi_image_free(texture->TMP_ImageByteData);
	//texture->TMP_ImageByteData = nullptr;
	glBindTexture(GL_TEXTURE_2D, 0);
}

TextureManager::TextureManager()
	: m_Textures{}, m_TexPromises{}
{
	this->LoadTextureFromPath(MissingTexPath, false);
}

TextureManager* TextureManager::Get()
{
	static TextureManager instance;
	return &instance;
}

static uint8_t* loadImageData(const char* ImagePath, int* ImageWidth, int* ImageHeight, int* ImageColorChannels)
{
	stbi_set_flip_vertically_on_load(true);

	uint8_t* imageData = stbi_load(ImagePath, ImageWidth, ImageHeight, ImageColorChannels, 0);
	
	return imageData;
}

static Texture* asyncTextureLoader(
	ImageLoader_t ImageLoader,
	Texture* AsyncTexture,
	std::string ActualPath,
	uint32_t ResourceId
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
	AsyncTexture->ResourceId = ResourceId;
	AsyncTexture->ImagePath = ActualPath;

	return AsyncTexture;
}

uint32_t TextureManager::LoadTextureFromPath(const std::string& Path, bool ShouldLoadAsync)
{
	std::string ResDir = EngineJsonConfig["ResourcesDirectory"];
	std::string ActualPath = ResDir + Path;

	auto it = m_StringToTextureId.find(Path);
	
	if (it == m_StringToTextureId.end())
	{
		static uint32_t CurrentResourceId = 1;

		uint32_t newResourceId = CurrentResourceId;
		CurrentResourceId += 1;

		m_StringToTextureId.insert(std::pair(Path, newResourceId));

		uint32_t newGpuId;
		glGenTextures(1, &newGpuId);

		Texture newTexture{ Path, newResourceId, newGpuId };

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

			Texture* asyncTexture = new Texture;

			std::promise<Texture*>* promise = new std::promise<Texture*>();

			std::thread(
				[promise, asyncTexture, ActualPath, newResourceId](auto asyncFunc, auto imageLoader)
				{
					promise->set_value_at_thread_exit(asyncFunc(imageLoader, asyncTexture, ActualPath, newResourceId));
				}
			, asyncTextureLoader, loadImageData).detach();

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

			registerTexture(newTexture);
		}

		m_Textures.insert(std::pair(newResourceId, newTexture));

		return newResourceId;
	}
	else
		return it->second;
}

Texture* TextureManager::GetTextureResource(uint32_t ResourceId)
{
	auto textureIt = m_Textures.find(ResourceId);
	if (textureIt != m_Textures.end())
		return &textureIt->second;
	else
		return nullptr;
}

void TextureManager::FinalizeAsyncLoadedTextures()
{
	if (m_TexPromises.size() == 0)
		return;

	for (size_t promiseIndex = 0; promiseIndex < m_TexPromises.size(); promiseIndex++)
	{
		std::promise<Texture*>* promise = m_TexPromises[promiseIndex];
		auto& f = m_TexFutures[promiseIndex];

		// https://stackoverflow.com/a/10917945/16875161
		// TODO 10/09/2024
		// Surely there's gotta be a better way than this `wait_for` bullshit
		if (!f.valid() || (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready))
			continue;

		Texture* loadedImage = f.get();

		auto imageIt = m_Textures.find(loadedImage->ResourceId);

		if (imageIt == m_Textures.end())
		{
			Debug::Log(std::vformat(
				"Got async loaded texture with invalid Resource ID {}",
				std::make_format_args(loadedImage->ResourceId)
			));
			continue;
		}
		
		Texture& image = imageIt->second;

		if (image.Status != TextureLoadStatus::InProgress)
			continue;

		image.Width = loadedImage->Width;
		image.Height = loadedImage->Height;
		image.NumColorChannels = loadedImage->NumColorChannels;
		image.Status = loadedImage->Status;

		size_t bufSize = (size_t)image.Width * (size_t)image.Height * (size_t)image.NumColorChannels;

		image.TMP_ImageByteData = (uint8_t*)malloc(bufSize);

		if (!image.TMP_ImageByteData)
		{
			Debug::Log(std::vformat(
				"`malloc` failed in ::FinalizeAsyncLoadedTextures (Requested amount was {} bytes)",
				std::make_format_args(bufSize)
			));
			Debug::Save();

			throw("Could not allocate a buffer to copy image from async thread");
		}
		
		memcpy(image.TMP_ImageByteData, loadedImage->TMP_ImageByteData, bufSize);
		
		//stbi_image_free(loadedImage->TMP_ImageByteData);

		//delete loadedImage;

		registerTexture(image);
		
		m_TexPromises.erase(m_TexPromises.begin() + promiseIndex);
		m_TexFutures.erase(m_TexFutures.begin() + promiseIndex);

		delete promise;
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