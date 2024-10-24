#pragma once

#include<vector>

#include"datatype/Mesh.hpp"

class VBO
{
public:
	VBO();

	void SetBufferData(const std::vector<Vertex>& Vertices) const;

	void Bind() const;
	void Unbind();
	void Delete() const;

	uint32_t ID = 0;
};

class VAO
{
public:
	VAO();

	void LinkAttrib(
		VBO& VertexBuffer,
		uint32_t Layout,
		uint32_t Components,
		uint32_t Type,
		int32_t Stride,
		void* Offset
	);

	void Bind() const;
	void Unbind();
	void Delete() const;

	uint32_t ID = 0;
};

class EBO
{
public:
	EBO();

	void SetBufferData(const std::vector<uint32_t>& Indices) const;

	void Bind() const;
	void Unbind();
	void Delete() const;

	uint32_t ID = 0;
};

class FBO
{
public:
	FBO(int Width, int Height, int MSSamples = 0, bool AttachRenderBuffer = true);

	void Bind() const;
	void Unbind();
	void Delete() const;

	void BindTexture() const;
	void UnbindTexture();

	int MSAASamples = 0;

	uint32_t ID = 0;

	uint32_t RenderBufferID = 0;
	uint32_t TextureID = 0;
};
