#pragma once

#include<vector>
#include<future>

#define TEXTUREMANAGER_INVALID_ID 0xFFFFFFu

enum class MaterialTextureType { NotAssigned, Diffuse, Specular };

struct Texture
{
	uint32_t Identifier = TEXTUREMANAGER_INVALID_ID;
	MaterialTextureType Usage = MaterialTextureType::Diffuse;

	std::string ImagePath = "";
	int ImageHeight = -1;
	int ImageWidth = -1;
	int ImageNumColorChannels = -1;

	bool AttemptedLoad = false;

	// De-allocated after the Texture is uploaded to the GPU
	uint8_t* TMP_ImageByteData = nullptr;
};

class TextureManager
{
public:
	TextureManager();

	static TextureManager* Get();

	/*
	Goes through all images which are done loading asynchronously and instantiates their texture objects with the ID.
	*/
	void FinalizeAsyncLoadedTextures();

	/*
	Load texture data from an image file and upload it to the GPU as a GL_TEXTURE_2D
	@param The image path
	@param Should it be loaded in a separate thread without freezing the game
	If ShouldLoadAsync is true, then Texture->Identifier will be TEXTUREMANAGER_INVALID_ID until
	the Texture is ready to use.
	*/
	Texture* LoadTextureFromPath(const std::string& Path, bool ShouldLoadAsync = true);

	/*
	Load an image's data, used internally by TextureManager::CreateTexture2D and is a separate function for asynchronous loading
	*/
	uint8_t* LoadImageData(const char* ImagePath, int* ImageWidth, int* ImageHeight, int* ImageColorChannels);

	static TextureManager* Singleton;

private:
	std::vector<std::promise<Texture*>*> m_TexPromises;
	std::unordered_map<std::string, Texture*> m_Textures;
};
