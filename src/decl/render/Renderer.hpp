#pragma once

#include <unordered_set>
#include <optional>
#include <glm/mat4x4.hpp>

#include "render/ShaderManager.hpp"
#include "render/GpuBuffers.hpp"

#include "gameobject/Base3D.hpp"

/*
	09/09/2024
	Why can't SDL have something like OpenGL's Debug Log?
	They have their own logging stuff, but it doesn't seem to do anything with errors
*/
#define PHX_SDL_CALL(func, ...)                                  \
{                                                                \
	int status = func(__VA_ARGS__);                              \
	std::string funcName = #func;                                \
	if (status < 0)                                              \
	{                                                            \
		const char* errMessage = SDL_GetError();                 \
		throw(std::vformat(                                      \
			"Error in {}:\nCode: {}\nMessage: {}",               \
			std::make_format_args(funcName, status, errMessage)  \
		));                                                      \
	}                                                            \
}                                                                \

struct RenderItem
{
	uint32_t RenderMeshId{};
	glm::mat4 Transform{};
	glm::vec3 Size;
	uint32_t MaterialId{};
	Color TintColor;
	float Transparency{};
	float Reflectivity{};
	
	FaceCullingMode FaceCulling = FaceCullingMode::BackFace;
};

enum class LightType : uint8_t { Directional, Point, Spot };

struct LightItem
{
	LightType Type = LightType::Point;

	Vector3 Position;
	Color LightColor;
	float Range = 60.f;

	glm::mat4 ShadowMapProjection{};
	bool HasShadowMap = false;
	int ShadowMapIndex{};
	int ShadowMapTextureId{};
};

struct Scene
{
	std::vector<RenderItem> RenderList;
	std::vector<LightItem> LightingList;

	std::unordered_set<uint32_t> UsedShaders;
};

class Renderer
{
public:
	Renderer(uint32_t Width, uint32_t Height, SDL_Window* Window);
	~Renderer();

	// Changes the rendering resolution
	void ChangeResolution(uint32_t newWidth, uint32_t newHeight);

	void DrawScene(const Scene& Scene);

	// Submits a single draw call
	// `NumInstances` made under the assumption the caller
	// has bound the Instanced Array prior to calling this function
	void DrawMesh(
		const Mesh& Object,
		ShaderProgram& Shader,
		const Vector3& Size,
		const glm::mat4& Transform = glm::mat4(1.0f),
		FaceCullingMode Culling = FaceCullingMode::BackFace,
		int32_t NumInstances = 1
	);

	void SwapBuffers();

	GpuFrameBuffer* Framebuffer = nullptr;

	SDL_GLContext GLContext = nullptr;

private:
	void m_SetMaterialData(const RenderItem&);

	GpuVertexArray* m_VertexArray = nullptr;
	GpuVertexBuffer* m_VertexBuffer = nullptr;
	GpuElementBuffer* m_ElementBuffer = nullptr;
	uint32_t m_InstancingBuffer{};

	SDL_Window* m_Window = nullptr;
	
	uint32_t m_Width, m_Height = 0;

	int m_MsaaSamples = 0;
};
