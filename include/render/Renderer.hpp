#pragma once

#define SHADER_MAX_LIGHTS 6

#include<glm/matrix.hpp>

#include"render/ShaderProgram.hpp"
#include"render/Material.hpp"
#include"datatype/Mesh.hpp"
#include"datatype/Buffer.hpp"
#include"datatype/Color.hpp"
#include"datatype/Vector3.hpp"
#include"gameobject/Base3D.hpp"

struct MeshData_t
{
	Mesh* MeshData{};
	glm::mat4 Transform{};
	Vector3 Size;
	RenderMaterial* Material{};
	Color TintColor;
	float Transparency{};
	float Reflectivity{};
	
	FaceCullingMode FaceCulling = FaceCullingMode::BackFace;
};

enum class LightType { DirectionalLight, Pointlight, Spotlight };

struct LightData_t
{
	LightType Type = LightType::Pointlight;

	Vector3 Position;
	Color LightColor;
	float Range = 60.f;

	glm::mat4 ShadowMapProjection{};
	bool HasShadowMap = false;
	int ShadowMapIndex{};
	int ShadowMapTextureId{};
};

struct Scene_t
{
	std::vector<MeshData_t> MeshData;
	std::vector<MeshData_t> TransparentMeshData;
	std::vector<LightData_t> LightData;

	std::vector<ShaderProgram*> UniqueShaders;
};

class Renderer
{
public:
	Renderer(uint32_t Width, uint32_t Height, SDL_Window* Window);

	// Changes the rendering resolution
	void ChangeResolution(uint32_t newWidth, uint32_t newHeight);

	void DrawScene(Scene_t& Scene);

	// Submits a single draw call
	void DrawMesh(
		Mesh* Object,
		ShaderProgram* Shaders,
		Vector3 Size,
		glm::mat4 Transform = glm::mat4(1.0f),
		FaceCullingMode Culling = FaceCullingMode::BackFace
	);

	void SwapBuffers();

	FBO* Framebuffer = nullptr;

	SDL_GLContext GLContext = nullptr;

private:
	void m_SetTextureUniforms(MeshData_t& Mesh, ShaderProgram* Shaders);

	VAO* m_VertexArray = nullptr;
	VBO* m_VertexBuffer = nullptr;
	EBO* m_ElementBuffer = nullptr;

	SDL_Window* m_Window = nullptr;
	
	uint32_t m_Width, m_Height = 0;

	int m_MsaaSamples = 0;
};
