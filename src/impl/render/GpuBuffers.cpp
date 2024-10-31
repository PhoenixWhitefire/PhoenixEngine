#include <format>
#include <glad/gl.h>

#include "render/GpuBuffers.hpp"

GpuVertexArray::GpuVertexArray()
{
	glGenVertexArrays(1, &m_GpuId);
}

GpuVertexArray::~GpuVertexArray()
{
	glDeleteVertexArrays(1, &m_GpuId);
}

void GpuVertexArray::LinkAttrib(
	GpuVertexBuffer& VertexBuffer,
	uint32_t Layout,
	uint32_t Components,
	uint32_t Type,
	int32_t Stride,
	void* Offset
)
{
	VertexBuffer.Bind();

	glVertexAttribPointer(Layout, Components, Type, GL_FALSE, Stride, Offset);
	glEnableVertexAttribArray(Layout);

	VertexBuffer.Unbind();
}

void GpuVertexArray::Bind()
{
	glBindVertexArray(m_GpuId);
}

void GpuVertexArray::Unbind()
{
	glBindVertexArray(0);
}

GpuVertexBuffer::GpuVertexBuffer()
{
	glGenBuffers(1, &m_GpuId);
	this->Bind();

	glBufferData(GL_ARRAY_BUFFER, 0, {}, GL_STREAM_DRAW);
}

GpuVertexBuffer::~GpuVertexBuffer()
{
	glDeleteBuffers(1, &m_GpuId);
}

void GpuVertexBuffer::SetBufferData(const std::vector<Vertex>& Vertices, BufferUsageHint UsageHint)
{
	this->Bind();

	glBufferData(
		GL_ARRAY_BUFFER,
		Vertices.size() * sizeof(Vertex),
		Vertices.data(),
		UsageHint == BufferUsageHint::Dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW
	);
}

void GpuVertexBuffer::Bind()
{
	glBindBuffer(GL_ARRAY_BUFFER, m_GpuId);
}

void GpuVertexBuffer::Unbind()
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GpuElementBuffer::GpuElementBuffer()
{
	glGenBuffers(1, &m_GpuId);
	this->Bind();

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, {}, GL_STREAM_DRAW);
}

GpuElementBuffer::~GpuElementBuffer()
{
	glDeleteBuffers(1, &m_GpuId);
}

void GpuElementBuffer::SetBufferData(const std::vector<GLuint>& Indices, BufferUsageHint UsageHint)
{
	this->Bind();

	glBufferData(
		GL_ELEMENT_ARRAY_BUFFER,
		Indices.size() * sizeof(GLuint),
		Indices.data(),
		UsageHint == BufferUsageHint::Dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW
	);
}

void GpuElementBuffer::Bind()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_GpuId);
}

void GpuElementBuffer::Unbind()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

GpuFrameBuffer::GpuFrameBuffer(int Width, int Height, int MSSamples, bool AttachRenderBuffer)
{
	glGenFramebuffers(1, &m_GpuId);
	
	GLenum binding = MSSamples > 0 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

	glActiveTexture(GL_TEXTURE0);

	glGenTextures(1, &m_TextureId);
	glBindTexture(binding, m_TextureId);
	
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
		glTexParameteri(binding, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

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
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TextureId, 0);

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

GpuFrameBuffer::~GpuFrameBuffer()
{
	glDeleteTextures(1, &m_TextureId);
	
	if (m_RenderBufferId)
		glDeleteRenderbuffers(1, &m_RenderBufferId);

	glDeleteFramebuffers(1, &m_GpuId);
}

void GpuFrameBuffer::UpdateResolution(int Width, int Height)
{
	this->Bind();
	this->BindTexture();

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

void GpuFrameBuffer::Bind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_GpuId);
}

void GpuFrameBuffer::Unbind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GpuFrameBuffer::BindTexture()
{
	glBindTexture(/*this->MSAASamples > 0 ? GL_TEXTURE_2D_MULTISAMPLE : */ GL_TEXTURE_2D, m_TextureId);
}

void GpuFrameBuffer::UnbindTexture()
{
	glBindTexture(/*this->MSAASamples > 0 ? GL_TEXTURE_2D_MULTISAMPLE : */ GL_TEXTURE_2D, 0);
}
