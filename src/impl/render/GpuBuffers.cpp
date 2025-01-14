#include <format>
#include <glad/gl.h>
#include <tracy/Tracy.hpp>

#include "render/GpuBuffers.hpp"

void GpuVertexArray::Initialize()
{
	ZoneScoped;

	glGenVertexArrays(1, &m_GpuId);
}

void GpuVertexArray::Delete()
{
	ZoneScoped;

	glDeleteVertexArrays(1, &m_GpuId);
	m_GpuId = UINT32_MAX;
}

void GpuVertexArray::LinkAttrib(
	GpuVertexBuffer& VertexBuffer,
	uint32_t Layout,
	uint32_t Components,
	uint32_t Type,
	int32_t Stride,
	void* Offset
) const
{
	ZoneScoped;

	this->Bind();
	VertexBuffer.Bind();

	glVertexAttribPointer(Layout, Components, Type, GL_FALSE, Stride, Offset);
	glEnableVertexAttribArray(Layout);

	VertexBuffer.Unbind();
	this->Unbind();
}

void GpuVertexArray::Bind() const
{
	ZoneScoped;

	glBindVertexArray(m_GpuId);
}

void GpuVertexArray::Unbind() const
{
	ZoneScoped;

	glBindVertexArray(0);
}

void GpuVertexBuffer::Initialize()
{
	ZoneScoped;

	glGenBuffers(1, &m_GpuId);
	this->Bind();

	glBufferData(GL_ARRAY_BUFFER, 0, {}, GL_STREAM_DRAW);
}

void GpuVertexBuffer::Delete()
{
	ZoneScoped;

	glDeleteBuffers(1, &m_GpuId);
	m_GpuId = UINT32_MAX;
}

void GpuVertexBuffer::SetBufferData(const std::vector<Vertex>& Vertices, BufferUsageHint UsageHint) const
{
	ZoneScoped;

	this->Bind();

	glBufferData(
		GL_ARRAY_BUFFER,
		Vertices.size() * sizeof(Vertex),
		Vertices.data(),
		UsageHint == BufferUsageHint::Dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW
	);

	this->Unbind();
}

void GpuVertexBuffer::Bind() const
{
	ZoneScoped;

	glBindBuffer(GL_ARRAY_BUFFER, m_GpuId);
}

void GpuVertexBuffer::Unbind() const
{
	ZoneScoped;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GpuElementBuffer::Initialize()
{
	ZoneScoped;

	glGenBuffers(1, &m_GpuId);
	this->Bind();

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, {}, GL_STREAM_DRAW);

	this->Unbind();
}

void GpuElementBuffer::Delete()
{
	ZoneScoped;

	glDeleteBuffers(1, &m_GpuId);
	m_GpuId = UINT32_MAX;
}

void GpuElementBuffer::SetBufferData(const std::vector<GLuint>& Indices, BufferUsageHint UsageHint) const
{
	ZoneScoped;

	this->Bind();

	glBufferData(
		GL_ELEMENT_ARRAY_BUFFER,
		Indices.size() * sizeof(GLuint),
		Indices.data(),
		UsageHint == BufferUsageHint::Dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW
	);

	this->Unbind();
}

void GpuElementBuffer::Bind() const
{
	ZoneScoped;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_GpuId);
}

void GpuElementBuffer::Unbind() const
{
	ZoneScoped;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

GpuFrameBuffer::GpuFrameBuffer(int TargetWidth, int TargetHeight, int MSSamples, bool AttachRenderBuffer)
{
	ZoneScoped;

	this->Initialize(TargetWidth, TargetHeight, MSSamples, AttachRenderBuffer);
}

void GpuFrameBuffer::Initialize(int TargetWidth, int TargetHeight, int MSSamples, bool AttachRenderBuffer)
{
	ZoneScoped;

	this->Width = TargetWidth, this->Height = TargetHeight;

	glGenFramebuffers(1, &m_GpuId);
	
	GLenum binding = MSSamples > 0 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

	glActiveTexture(GL_TEXTURE0);

	glGenTextures(1, &GpuTextureId);
	glBindTexture(binding, GpuTextureId);
	
	if (MSSamples > 0)
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSSamples, GL_RGB, Width, Height, GL_TRUE);
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, Width, Height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	if (binding != GL_TEXTURE_2D_MULTISAMPLE)
	{
		glTexParameteri(binding, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(binding, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexParameteri(binding, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(binding, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	this->Bind();

	// TODO fix shadowmap specific stuff!

	/*
	if (IsShadowMap)
	{
		GLfloat shadowmapclampcol[] = { 1.0f, 1.0f, 1.0f, 1.0f };

		glTexParameterfv(Binding, GL_TEXTURE_BORDER_COLOR, shadowmapclampcol);

		//TODO fix stuff for shadow map (missing in SDL opengl)
		
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		
	}
	*/

	if (AttachRenderBuffer)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GpuTextureId, 0);

		glGenRenderbuffers(1, &m_RenderBufferId);
		glBindRenderbuffer(GL_RENDERBUFFER, m_RenderBufferId);

		if (MSSamples > 0)
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, MSSamples, GL_DEPTH24_STENCIL8, Width, Height);
		else
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, Width, Height);

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RenderBufferId);
	}

	auto FrameBufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (FrameBufferStatus != GL_FRAMEBUFFER_COMPLETE)
		throw(std::vformat("Could not create a framebuffer, error ID: {}", std::make_format_args(FrameBufferStatus)));
}

void GpuFrameBuffer::Delete()
{
	ZoneScoped;

	glDeleteTextures(1, &GpuTextureId);
	
	if (m_RenderBufferId)
		glDeleteRenderbuffers(1, &m_RenderBufferId);

	glDeleteFramebuffers(1, &m_GpuId);

	this->GpuTextureId = UINT32_MAX;
	m_RenderBufferId = UINT32_MAX;
	m_GpuId = UINT32_MAX;
}

void GpuFrameBuffer::UpdateResolution(int NewWidth, int NewHeight)
{
	ZoneScoped;

	this->Bind();
	this->BindTexture();

	this->Width = NewWidth, this->Height = NewHeight;

	// "TODO fix msaa"
	// 21/10/2024:
	// just comment it out for now
	// idk when that og comment was written,
	// more than 2 years ago at least

	//if (this->MSAASamples > 0)
	//	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_MsaaSamples, GL_RGB, m_Width, m_Height, GL_TRUE);
	//else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, Width, Height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glBindRenderbuffer(GL_RENDERBUFFER, m_RenderBufferId);

	//if (this->MSAASamples > 0)
	//	glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_MsaaSamples, GL_DEPTH32F_STENCIL8, m_Width, m_Height);
	//else
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, Width, Height);
}

void GpuFrameBuffer::Bind() const
{
	ZoneScoped;

	glBindFramebuffer(GL_FRAMEBUFFER, m_GpuId);
}

void GpuFrameBuffer::Unbind() const
{
	ZoneScoped;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GpuFrameBuffer::BindTexture() const
{
	ZoneScoped;

	glBindTexture(/*this->MSAASamples > 0 ? GL_TEXTURE_2D_MULTISAMPLE : */ GL_TEXTURE_2D, GpuTextureId);
}

void GpuFrameBuffer::UnbindTexture() const
{
	ZoneScoped;

	glBindTexture(/*this->MSAASamples > 0 ? GL_TEXTURE_2D_MULTISAMPLE : */ GL_TEXTURE_2D, 0);
}
