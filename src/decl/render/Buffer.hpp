#pragma once

#include<vector>

#include"datatype/Mesh.hpp"

enum class BufferUsageHint { Static, Dynamic };

struct VBO
{
	VBO();
	~VBO();

	void SetBufferData(
		const std::vector<Vertex>& Vertices,
		BufferUsageHint UsageHint = BufferUsageHint::Dynamic
	) const;

	void Bind() const;
	void Unbind();

	uint32_t ID = 0;
};

struct VAO
{
	VAO();
	~VAO();

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

	uint32_t ID = 0;
};

struct EBO
{
	EBO();
	~EBO();

	void SetBufferData(
		const std::vector<uint32_t>& Indices,
		BufferUsageHint UsageHint = BufferUsageHint::Dynamic
	) const;

	void Bind() const;
	void Unbind();

	uint32_t ID = 0;
};

struct FBO
{
	FBO(int Width, int Height, int MSSamples = 0, bool AttachRenderBuffer = true);
	~FBO();

	void Bind() const;
	void Unbind();

	void BindTexture() const;
	void UnbindTexture();

	int MSAASamples = 0;

	uint32_t ID = 0;

	uint32_t RenderBufferID = 0;
	uint32_t TextureID = 0;
};
