#pragma once

#define SHADER_MAX_LIGHTS 6

#include<glm/matrix.hpp>

#include"render/ShaderProgram.hpp"
#include"render/Material.hpp"
#include"datatype/Mesh.hpp"
#include"datatype/Buffer.hpp"
#include"datatype/Color.hpp"
#include"datatype/Vector3.hpp"

struct MeshData_t
{
	Mesh* MeshData = nullptr;
	RenderMaterial* Material;
	Vector3 Size;
	Color TintColor;
	float Transparency = 0.f;
	float Reflectivity = 0.f;
	glm::mat4 Matrix = glm::mat4(1.0f);
};

enum class LightType { DirectionalLight, Pointlight, Spotlight };

struct LightData_t
{
	LightType Type = LightType::Pointlight;

	Vector3 Position;
	Color LightColor;
	float Range = 60;

	glm::mat4 ShadowMapProjection;
	bool HasShadowMap = false;
	int ShadowMapIndex = 0;
	int ShadowMapTextureId = 0;
};

struct Scene_t
{
	std::vector<MeshData_t> MeshData;
	std::vector<MeshData_t> TransparentMeshData;
	std::vector<LightData_t> LightData;

	ShaderProgram* Shaders = (ShaderProgram*)nullptr;
};

class Renderer
{
public:
	Renderer(uint32_t Width, uint32_t Height, SDL_Window* Window, uint8_t MSAASamples);

	// Changes the rendering resolution
	void ChangeResolution(uint32_t newWidth, uint32_t newHeight);

	void DrawScene(Scene_t& Scene);

	// Submits a single draw call
	void DrawMesh(
		Mesh* Object,
		ShaderProgram* Shaders,
		Vector3 Size,
		glm::mat4 Matrix = glm::mat4(1.0f)
	);

	void SwapBuffers();

	FBO* m_framebuffer = nullptr;

	SDL_GLContext m_glContext;

private:
	void m_setTextureUniforms(MeshData_t& Mesh, ShaderProgram* Shaders);

	VAO* m_vertexArray = nullptr;
	VBO* m_vertexBuffer = nullptr;
	EBO* m_elementBuffer = nullptr;

	SDL_Window* m_window;
	
	uint32_t m_width, m_height;

	int m_msaaSamples;
};
