#pragma once

#include<glm/matrix.hpp>

#include"render/ShaderProgram.hpp"
#include"render/Material.hpp"
#include"datatype/Mesh.hpp"
#include"datatype/Buffer.hpp"
#include"datatype/Color.hpp"
#include"datatype/Vector3.hpp"
#include"gameobject/Base3D.hpp"

#define SHADER_MAX_LIGHTS 6

/*
	09/09/2024
	Why can't SDL have something like OpenGL's Debug Log?
	They have their own logging stuff, but it doesn't seem to do anything with errors
*/
#define PHX_SDL_CALL(func, ...) \
{ \
	int status = func(__VA_ARGS__); \
	if (status < 0) \
	{ \
		const char* errMessage = SDL_GetError(); \
		throw(std::vformat( \
			std::string("Error in ") + std::string(#func) + std::string(":\nCode: {}\nMessage: {}"), \
			std::make_format_args(status, errMessage) \
		)); \
	} \
} \

struct RenderItem
{
	Mesh* RenderMesh{};
	glm::mat4 Transform{};
	Vector3 Size;
	RenderMaterial* Material{};
	Color TintColor;
	float Transparency{};
	float Reflectivity{};
	
	FaceCullingMode FaceCulling = FaceCullingMode::BackFace;
};

enum class LightType { Directional, Point, Spot };

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

	std::vector<ShaderProgram*> UniqueShaders;
};

class Renderer
{
public:
	Renderer(uint32_t Width, uint32_t Height, SDL_Window* Window);

	// Changes the rendering resolution
	void ChangeResolution(uint32_t newWidth, uint32_t newHeight);

	void DrawScene(const Scene& Scene);

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
	void m_SetTextureUniforms(const RenderItem& Mesh, ShaderProgram* Shaders);

	VAO* m_VertexArray = nullptr;
	VBO* m_VertexBuffer = nullptr;
	EBO* m_ElementBuffer = nullptr;

	SDL_Window* m_Window = nullptr;
	
	uint32_t m_Width, m_Height = 0;

	int m_MsaaSamples = 0;
};
