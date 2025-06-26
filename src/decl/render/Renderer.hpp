#pragma once

#include <unordered_set>
#include <optional>
#include <glm/mat4x4.hpp>
#include <SDL3/SDL_video.h>

#include "render/RendererScene.hpp"
#include "render/GpuBuffers.hpp"
#include "asset/ShaderManager.hpp"

/*
	09/09/2024
	Why can't SDL have something like OpenGL's Debug Log?
	They have their own logging stuff, but it doesn't seem to do anything with errors
*/
#define PHX_SDL_CALL(func, ...)                                  \
{                                                                \
	bool success = func(__VA_ARGS__);                            \
	std::string funcName = #func;                                \
	if (!success)                                                \
		throw(std::format(                                       \
			"Error in " #func ":\nMessage: {}",                  \
			SDL_GetError()                                       \
		));                                                      \
}                                                                \

class Renderer
{
public:
	Renderer() = default;
	Renderer(uint32_t Width, uint32_t Height, SDL_Window* Window);
	~Renderer();

	void Initialize(uint32_t Width, uint32_t Height, SDL_Window* Window);

	// Changes the rendering resolution
	void ChangeResolution(uint32_t newWidth, uint32_t newHeight);

	void DrawScene(
		const Scene& Scene,
		const glm::mat4& RenderMatrix,
		const glm::mat4& CameraTransform,
		double RunningTime,
		bool DebugWireframeRendering = false
	);

	// Submits a single draw call
	// `NumInstances` made under the assumption the caller
	// has bound the Instanced Array prior to calling this function
	void DrawMesh(
		const Mesh& Object,
		ShaderProgram& Shader,
		const glm::vec3& Size,
		const glm::mat4& Transform = glm::mat4(1.f),
		FaceCullingMode Culling = FaceCullingMode::BackFace,
		int32_t NumInstances = 1
	);

	void SwapBuffers();

	GpuFrameBuffer FrameBuffer;

	SDL_GLContext GLContext = nullptr;

	uint32_t AccumulatedDrawCallCount = 0;

private:
	void m_SetMaterialData(const RenderItem&, bool DebugWireframeRendering);

	GpuVertexArray m_VertexArray;
	GpuVertexBuffer m_VertexBuffer;
	GpuElementBuffer m_ElementBuffer;
	uint32_t m_InstancingBuffer{};

	SDL_Window* m_Window = nullptr;
	
	uint32_t m_Width = 0, m_Height = 0;

	int m_MsaaSamples = 0;
};
