#pragma once

#include <vector>

#include "datatype/Mesh.hpp"

enum class BufferUsageHint : uint8_t { Static, Dynamic };

class GpuVertexBuffer
{
public:
	GpuVertexBuffer();
	~GpuVertexBuffer();

	void SetBufferData(
		const std::vector<Vertex>& Vertices,
		BufferUsageHint UsageHint = BufferUsageHint::Dynamic
	);

	void Bind();
	void Unbind();

private:
	uint32_t m_GpuId = UINT32_MAX;
};

class GpuVertexArray
{
public:
	GpuVertexArray();
	~GpuVertexArray();

	void LinkAttrib(
		GpuVertexBuffer& VertexBuffer,
		uint32_t Layout,
		uint32_t Components,
		uint32_t Type,
		int32_t Stride,
		void* Offset
	);

	void Bind();
	void Unbind();

private:
	uint32_t m_GpuId = UINT32_MAX;
};

class GpuElementBuffer
{
public:
	GpuElementBuffer();
	~GpuElementBuffer();

	void SetBufferData(
		const std::vector<uint32_t>& Indices,
		BufferUsageHint UsageHint = BufferUsageHint::Dynamic
	);

	void Bind();
	void Unbind();

private:
	uint32_t m_GpuId = UINT32_MAX;
};

class GpuFrameBuffer
{
public:
	GpuFrameBuffer(int Width, int Height, int MSSamples = 0, bool AttachRenderBuffer = true);
	~GpuFrameBuffer();

	void Bind();
	void Unbind();

	void BindTexture();
	void UnbindTexture();

	void UpdateResolution(int Width, int Height);

private:
	uint32_t m_GpuId = UINT32_MAX;

	uint32_t m_RenderBufferId = UINT32_MAX;
	uint32_t m_TextureId = UINT32_MAX;
};
