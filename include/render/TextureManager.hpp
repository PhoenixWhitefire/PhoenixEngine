#pragma once

#include<stb_image.h>
#include"ShaderProgram.hpp"

#include<ThreadManager.hpp>

enum class TextureType { NOTASSIGNED, DIFFUSE, SPECULAR };

class Texture
{
public:
	GLuint Identifier = 0;
	TextureType Usage = TextureType::DIFFUSE;

	std::string ImagePath = "";
	int ImageHeight = -1;
	int ImageWidth = -1;
	int ImageNumColorChannels = -1;

	bool AttemptedLoad = false;

	unsigned char* TMP_ImageByteData = nullptr;
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
	@param Should it be loaded in a separate thread without freezing the game
	If ShouldLoadAsync is true, then Texture->Identifier will be -1 and will change to the correct value when the image is finished loading
	*/
	void CreateTexture2D(Texture* TexturePtr, bool ShouldLoadAsync = true);

	/*
	Load an image's data, used internally by TextureManager::CreateTexture2D and is a separate function for asynchronous loading
	*/
	unsigned char* LoadImageData(const char* ImagePath, int* ImageWidth, int* ImageHeight, int* ImageColorChannels);

	static TextureManager* Singleton;

	std::vector<Texture*> TexturesToLoadAsync;
	std::vector<Texture*> AsyncLoadedTextures;
	
private:
	std::thread* AsyncTextureLoaderThread = nullptr;
};
