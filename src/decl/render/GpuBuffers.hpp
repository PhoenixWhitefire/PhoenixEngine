#pragma once

#include <vector>

#include "datatype/Mesh.hpp"

enum class BufferUsageHint : uint8_t { Static, Dynamic };

class GpuVertexBuffer
{
public:
	GpuVertexBuffer() = default;

	void Initialize();
	void Delete();

	void SetBufferData(
		const std::vector<Vertex>& Vertices,
		BufferUsageHint UsageHint = BufferUsageHint::Dynamic
	) const;

	void Bind() const;
	void Unbind() const;

private:
	uint32_t m_GpuId = UINT32_MAX;
};

class GpuVertexArray
{
public:
	GpuVertexArray() = default;

	void Initialize();
	void Delete();

	void LinkAttrib(
		GpuVertexBuffer& VertexBuffer,
		uint32_t Layout,
		uint32_t Components,
		uint32_t Type,
		int32_t Stride,
		void* Offset
	) const;

	void Bind() const;
	void Unbind() const;

private:
	uint32_t m_GpuId = UINT32_MAX;
};

class GpuElementBuffer
{
public:
	GpuElementBuffer() = default;

	void Initialize();
	void Delete();

	void SetBufferData(
		const std::vector<uint32_t>& Indices,
		BufferUsageHint UsageHint = BufferUsageHint::Dynamic
	) const;

	void Bind() const;
	void Unbind() const;

private:
	uint32_t m_GpuId = UINT32_MAX;
};

class GpuFrameBuffer
{
public:
	GpuFrameBuffer() = default;
	GpuFrameBuffer(int Width, int Height, int MSSamples = 0, bool DepthOnly = false);

	void Initialize(int Width, int Height, int MSSamples = 0, bool DepthOnly = false);
	void Delete();

	void Bind() const;
	void Unbind() const;

	void BindTexture() const;
	void UnbindTexture() const;

	void ChangeResolution(int Width, int Height);

	int Width = -1, Height = -1;
	uint32_t GpuTextureId = UINT32_MAX;

private:
	uint32_t m_GpuId = UINT32_MAX;
	uint32_t m_RenderBufferId = UINT32_MAX;
};
