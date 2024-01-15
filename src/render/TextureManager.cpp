#include<render/TextureManager.hpp>
#include<format>

TextureManager* TextureManager::Singleton = new TextureManager();

TextureManager* TextureManager::Get()
{
	if (!TextureManager::Singleton)
	{
		TextureManager::Singleton = new TextureManager();
	}

	return TextureManager::Singleton;
}

unsigned char* TextureManager::LoadImageData(const char* ImagePath, int* ImageWidth, int* ImageHeight, int* ImageColorChannels)
{
	stbi_set_flip_vertically_on_load(true);

	unsigned char* ImageData = stbi_load(ImagePath, ImageWidth, ImageHeight, ImageColorChannels, 0);
	
	return ImageData;
}

void asyncTextureLoader(void* ManagerPtr)
{
	TextureManager* Manager = (TextureManager*)ManagerPtr;

	Texture* Image = Manager->TexturesToLoadAsync[Manager->TexturesToLoadAsync.size() - 1];

	unsigned char* Data = Manager->LoadImageData(
	Image->ImagePath.c_str(),
		&Image->ImageWidth,
		&Image->ImageHeight,
		&Image->ImageNumColorChannels
	);

	Image->AttemptedLoad = true;

	Image->TMP_ImageByteData = Data;

	Manager->AsyncLoadedTextures.push_back(Image);
	Manager->TexturesToLoadAsync.pop_back();

	Image = nullptr;
}

void TextureManager::CreateTexture2D(Texture* TexturePtr, bool ShouldLoadAsync)
{
	if (TexturePtr->AttemptedLoad)
	{
		if (TexturePtr->TMP_ImageByteData == nullptr)
		{
			auto FormattedArgs = std::make_format_args(TexturePtr->ImagePath, (int)TexturePtr->Usage);
			Debug::Log(std::vformat("Failed to load texture '{}' (usage:{})", FormattedArgs));
			return;
		}

		glGenTextures(1, &TexturePtr->Identifier);
		glBindTexture(GL_TEXTURE_2D, TexturePtr->Identifier);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		// Specular textures may be in the form of PNGs, which have 4 color channels.
		// Manually set format if TexturePtr->Usage is specified
		// Set Format to 0 to make code try and identify usage through available color channels
		// TODO: recheck logic?
		GLenum Format = 0;

		switch (TexturePtr->ImageNumColorChannels)
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
					std::string("Invalid ImageNumColorChannels (was '{}') in TextureManager::CreateTexture2D!"),
					std::make_format_args(TexturePtr->ImageNumColorChannels)
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
			TexturePtr->ImageWidth,
			TexturePtr->ImageHeight,
			0,
			Format,
			GL_UNSIGNED_BYTE,
			TexturePtr->TMP_ImageByteData
		);

		glGenerateMipmap(GL_TEXTURE_2D);

		stbi_image_free(TexturePtr->TMP_ImageByteData);
		TexturePtr->TMP_ImageByteData = nullptr;
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	else
	{
		ShouldLoadAsync = false;

		if (!ShouldLoadAsync)
		{
			unsigned char* Data = this->LoadImageData(
				TexturePtr->ImagePath.c_str(),
				&TexturePtr->ImageWidth,
				&TexturePtr->ImageHeight,
				&TexturePtr->ImageNumColorChannels
			);

			TexturePtr->TMP_ImageByteData = Data;
			TexturePtr->AttemptedLoad = true;

			this->CreateTexture2D(TexturePtr, false);
		}
		else
		{
			Task* LoadTextureTask = new Task();
			LoadTextureTask->FuncArgument = (void*)this;
			LoadTextureTask->Function = asyncTextureLoader;
			LoadTextureTask->DbgInfo = "LoadTextureTask: Tex: " + TexturePtr->ImagePath;
			
			ThreadManager::Get()->DispatchJob(*LoadTextureTask);

			this->TexturesToLoadAsync.push_back(TexturePtr);
		}
	}
}

void TextureManager::FinalizeAsyncLoadedTextures()
{
	if (this->AsyncLoadedTextures.size() == 0)
		return;

	Texture* Image = this->AsyncLoadedTextures[this->AsyncLoadedTextures.size() - 1];

	if (Image->TMP_ImageByteData == nullptr)
	{
		Image->Identifier = 0xFFFFFFFF; //image failed to load

		Debug::Log(std::vformat("image failed to load: {}", std::make_format_args(Image->ImagePath)));

		this->AsyncLoadedTextures.pop_back();
	}

	this->CreateTexture2D(Image);

	this->AsyncLoadedTextures.pop_back();

	this->FinalizeAsyncLoadedTextures();
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