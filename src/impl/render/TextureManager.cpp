#include<stb_image.h>
#include<format>
#include<functional>
#include<glad/gl.h>

#include"render/TextureManager.hpp"
#include"render/ShaderProgram.hpp"
#include"GlobalJsonConfig.hpp"
#include"ThreadManager.hpp"
#include"Debug.hpp"

static const std::string MissingTexPath = "textures/MISSING2_MaximumADHD_status_1665776378145304579.png";
static const std::string TexLoadErr =
"Attempted to load texture '{}', but it failed. Additionally, the replacement Missing Texture could not be loaded.";

typedef std::function<Texture*(TextureManager*, Texture*, std::string)> AsyncTexLoader_t;

static void registerTexture(Texture* texture)
{
	if (texture->TMP_ImageByteData == nullptr)
	{
		auto FormattedArgs = std::make_format_args(texture->ImagePath);
		Debug::Log(std::vformat("Failed to load texture '{}'", FormattedArgs));

		if (texture->ImagePath != MissingTexPath)
		{
			Texture* replacement = TextureManager::Get()->LoadTextureFromPath(MissingTexPath, false);
			texture->Identifier = replacement->Identifier;
			texture->AttemptedLoad = true;
			texture->Height = replacement->Height;
			texture->Width = replacement->Width;
			texture->NumColorChannels = replacement->NumColorChannels;

			return;
		}
		else
			throw(std::vformat(TexLoadErr, std::make_format_args(texture->ImagePath)));
	}

	glGenTextures(1, &texture->Identifier);
	glBindTexture(GL_TEXTURE_2D, texture->Identifier);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	//printf("%s %i\n", texture->ImagePath.c_str(), texture->Identifier);

	// Specular textures may be in the form of PNGs, which have 4 color channels.
	// Manually set format if TexturePtr->Usage is specified
	// Set Format to 0 to make code try and identify usage through available color channels
	// TODO: recheck logic?
	GLenum Format = 0;

	switch (texture->NumColorChannels)
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
			std::make_format_args(texture->NumColorChannels, texture->ImagePath)
		));
		break;
	}

	};

	//if ((Format == GL_RGB) && TexturePtr->Usage == TextureType::SPECULAR)
	//	Format = GL_RED;

	// TODO: texture mipmaps, 4 is the number of mipmaps
	//glTexStorage2D(GL_TEXTURE_2D, 1, MipMapsInternalFormat, TexturePtr->ImageWidth, TexturePtr->ImageHeight);

	glTexImage2D
	(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		texture->Width,
		texture->Height,
		0,
		Format,
		GL_UNSIGNED_BYTE,
		texture->TMP_ImageByteData
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

uint8_t* TextureManager::LoadImageData(const char* ImagePath, int* ImageWidth, int* ImageHeight, int* ImageColorChannels)
{
	stbi_set_flip_vertically_on_load(true);

	uint8_t* ImageData = stbi_load(ImagePath, ImageWidth, ImageHeight, ImageColorChannels, 0);
	
	return ImageData;
}

static Texture* asyncTextureLoader(TextureManager* Manager, Texture* Image, std::string ActualPath)
{
	uint8_t* Data = Manager->LoadImageData(
		ActualPath.c_str(),
		&Image->Width,
		&Image->Height,
		&Image->NumColorChannels
	);

	Image->AttemptedLoad = true;

	Image->TMP_ImageByteData = Data;

	return Image;
}

Texture* TextureManager::LoadTextureFromPath(const std::string& Path, bool ShouldLoadAsync)
{
	std::string ResDir = EngineJsonConfig["ResourcesDirectory"];
	std::string ActualPath = ResDir + Path;

	auto it = m_Textures.find(Path);
	
	if (it == m_Textures.end())
	{
		Texture* texture = new Texture();
		texture->ImagePath = Path;

		if (Path != MissingTexPath)
			texture->Identifier = this->LoadTextureFromPath(MissingTexPath, false)->Identifier;

		m_Textures.insert(std::pair(Path, texture));

		//ShouldLoadAsync = false;

		if (ShouldLoadAsync)
		{
			/*Task* LoadTextureTask = new Task();
			LoadTextureTask->FuncArgument = (void*)this;
			LoadTextureTask->Function = asyncTextureLoader;
			LoadTextureTask->DbgInfo = "LoadTextureTask: Tex: " + ActualPath;

			ThreadManager::Get()->DispatchJob(*LoadTextureTask);*/

			std::promise<Texture*>* promise = new std::promise<Texture*>();

			std::thread(
				[promise, this, texture, ActualPath](AsyncTexLoader_t loader)
				{
					promise->set_value_at_thread_exit(loader(this, texture, ActualPath));
				}
			, asyncTextureLoader).detach();

			m_TexPromises.push_back(promise);
			m_TexFutures.push_back(promise->get_future().share());
		}
		else
		{
			uint8_t* Data = this->LoadImageData(
				ActualPath.c_str(),
				&texture->Width,
				&texture->Height,
				&texture->NumColorChannels
			);

			texture->TMP_ImageByteData = Data;
			texture->AttemptedLoad = true;

			registerTexture(texture);
		}

		return texture;
	}
	else
		return it->second;
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

		Texture* image = f.get();
		registerTexture(image);
		
		// remove element (ugly)
		// 10/09/2024
		auto lastPromise = m_TexPromises[m_TexPromises.size() - 1];
		m_TexPromises[m_TexPromises.size() - 1] = promise;
		m_TexPromises[promiseIndex] = lastPromise;

		m_TexPromises.pop_back();

		auto& lastFuture = m_TexFutures[m_TexFutures.size() - 1];
		m_TexFutures[m_TexFutures.size() - 1] = f;
		m_TexFutures[promiseIndex] = lastFuture;

		m_TexFutures.pop_back();

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