#pragma once

#include <vector>
#include <future>

struct Texture
{
	enum class DimensionType : uint8_t
	{
		Texture2D,
		Texture3D,
		TextureCube
	};

	enum class LoadStatus : uint8_t
	{
		NotAttempted,
		InProgress,
		Succeeded,
		Failed
	};

	std::string ImagePath{};

	uint32_t ResourceId = UINT32_MAX;
	uint32_t GpuId = UINT32_MAX;
	int Width = -1, Height = -1;
	int NumColorChannels = -1;

	DimensionType Type = DimensionType::Texture2D;
	LoadStatus Status = LoadStatus::NotAttempted;
	bool DoBilinearSmoothing = true;

	// De-allocated after the Texture is uploaded to the GPU
	uint8_t* TMP_ImageByteData{};
};

class TextureManager
{
public:
	static TextureManager* Get();
	static void Shutdown();

	/*
	Goes through all images which are done loading asynchronously and instantiates their texture objects with the ID.
	*/
	void FinalizeAsyncLoadedTextures();

	/*
	Load texture data from an image file and upload it to the GPU as a GL_TEXTURE_2D
	@param The image path
	@param Should it be loaded in a separate thread without freezing the game (default `true`)
	@return The Texture Resource ID (texture can be queried with `::GetTextureResource`)
	*/
	uint32_t LoadTextureFromPath(const std::string& Path, bool ShouldLoadAsync = true, bool DoBilinearSmoothing = true);

	/*
	Get a Texture by it's Resource ID
	*/
	Texture& GetTextureResource(uint32_t);

private:
	TextureManager();
	~TextureManager();

	void m_UploadTextureToGpu(Texture&);

	std::vector<Texture> m_Textures;
	std::unordered_map<std::string, uint32_t> m_StringToTextureId;

	std::vector<std::promise<Texture>*> m_TexPromises;
	std::vector<std::shared_future<Texture>> m_TexFutures;
};
