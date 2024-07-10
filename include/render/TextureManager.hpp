#pragma once

#include<vector>

enum class MaterialTextureType { NotAssigned, Diffuse, Specular };

class Texture
{
public:
	uint32_t Identifier = 0;
	MaterialTextureType Usage = MaterialTextureType::Diffuse;

	std::string ImagePath = "";
	int ImageHeight = -1;
	int ImageWidth = -1;
	int ImageNumColorChannels = -1;

	bool AttemptedLoad = false;

	uint8_t* TMP_ImageByteData = nullptr;
};

class TextureManager {
public:
	static TextureManager* Get();

	/*
	Goes through all images which are done loading asynchronously and instantiates their texture objects with the ID.
	*/
	void FinalizeAsyncLoadedTextures();

	/*
	Load texture data from an image file and upload it to the GPU as a GL_TEXTURE_2D
	@param A pointer to a Texture with Texture.ImagePath property set to the texture path
	@param Should it be loaded in a separate thread without freezing the game.
	If ShouldLoadAsync is true, then Texture->Identifier will be -1 and will change to the correct value
	when the image is finished loading
	*/
	void CreateTexture2D(Texture* TexturePtr, bool ShouldLoadAsync = true);

	/*
	Load an image's data, used internally by TextureManager::CreateTexture2D and is a separate function for asynchronous loading
	*/
	uint8_t* LoadImageData(const char* ImagePath, int* ImageWidth, int* ImageHeight, int* ImageColorChannels);

	static TextureManager* Singleton;

	std::vector<Texture*> TexturesToLoadAsync;
	std::vector<Texture*> AsyncLoadedTextures;
};
