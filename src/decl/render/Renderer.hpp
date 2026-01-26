#pragma once

#include <unordered_set>
#include <optional>
#include <glm/mat4x4.hpp>
#include <GLFW/glfw3.h>

#include "render/RendererScene.hpp"
#include "render/GpuBuffers.hpp"
#include "asset/ShaderManager.hpp"

class Renderer
{
public:
	Renderer() = default;
	Renderer(uint32_t Width, uint32_t Height, GLFWwindow* Window);
	void Shutdown();
	~Renderer();

	static Renderer* Get();

	void Initialize(uint32_t Width, uint32_t Height, GLFWwindow* Window);

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

	struct InstanceDrawInfo
	{
		glm::vec4 TransformRow1;
		glm::vec4 TransformRow2;
		glm::vec4 TransformRow3;
		glm::vec4 TransformRow4;
		glm::vec3 Scale;
		glm::vec3 Color;
		float Transparency;
	};
	static_assert(sizeof(InstanceDrawInfo) == ((4*4) + (3*2) + 1) * 4);

	GpuFrameBuffer FrameBuffer;

	GLFWwindow* Window = nullptr;

	uint32_t AccumulatedDrawCallCount = 0;
	uint32_t InstancingBuffer = UINT32_MAX;

private:
	void m_SetMaterialData(const RenderItem&, bool DebugWireframeRendering);

	GpuVertexArray m_VertexArray;
	GpuVertexBuffer m_VertexBuffer;
	GpuElementBuffer m_ElementBuffer;
	
	uint32_t m_Width = 0, m_Height = 0;

	int m_MsaaSamples = 0;
};
