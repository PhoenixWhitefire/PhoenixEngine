#pragma once

#define SHADER_MAX_LIGHTS 6

#include<glm/gtc/type_ptr.hpp>
#include<glm/gtc/matrix_transform.hpp>

#include"ShaderProgram.hpp"
#include<BaseMeshes.hpp>
#include<datatype/Mesh.hpp>
#include<datatype/Buffer.hpp>
#include<render/Material.hpp>

#include<render/GraphicsAbstractionLayer.hpp>

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

class Renderer {
	public:
		Renderer(unsigned int Width, unsigned int Height, SDL_Window* Window, int MSAASamples);

		// Changes the rendering resolution
		void ChangeResolution(unsigned int Width, unsigned int Height);

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

		unsigned int m_width, m_height;

		int m_msaaSamples;
};
