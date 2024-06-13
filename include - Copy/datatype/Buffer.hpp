#pragma once

#include<vector>

#include<glm/glm.hpp>

#include<render/GraphicsAbstractionLayer.hpp>
#include"Vector2.hpp"
#include"Vector3.hpp"
#include"Color.hpp"

#include<format>
#include"../Debug.hpp"

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 texUV;
};

class VBO {
public:
	VBO();

	void SetBufferData(std::vector<Vertex>& Vertices);

	void Bind();
	void Unbind();
	void Delete();

	GLuint ID;
};

class VAO {
public:
	VAO();

	void LinkAttrib(VBO& VertexBuffer, GLuint Layout, GLuint Components, GLenum Type, GLsizei Stride, void* Offset);

	void Bind();
	void Unbind();
	void Delete();

	GLuint ID;
};

class EBO {
public:
	EBO();

	void SetBufferData(std::vector<GLuint>& Indices);

	void Bind();
	void Unbind();
	void Delete();

	GLuint ID;
};

class FBO {
public:
	FBO(SDL_Window* Window, int Width, int Height, int MSSamples = 0, bool AttachRenderBuffer = true);

	void Bind();
	void Unbind();
	void Delete();

	void BindTexture();
	void UnbindTexture();

	int MSAASamples = 0;

	GLuint ID;

	GLuint RenderBufferID;
	GLuint TextureID;
};
