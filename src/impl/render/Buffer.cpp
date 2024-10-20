#include<format>
#include<glad/gl.h>

#include"render/Buffer.hpp"

VAO::VAO()
{
	glGenVertexArrays(1, &this->ID);
}

VAO::~VAO()
{
	glDeleteVertexArrays(1, &this->ID);
}

void VAO::LinkAttrib(
	VBO& VertexBuffer,
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

void VAO::Bind() const
{
	glBindVertexArray(this->ID);
}

void VAO::Unbind()
{
	glBindVertexArray(0);
}

VBO::VBO()
{
	glGenBuffers(1, &this->ID);
	this->Bind();
	glBufferData(GL_ARRAY_BUFFER, 0, {}, GL_STREAM_DRAW);
}

VBO::~VBO()
{
	glDeleteBuffers(1, &this->ID);
}

void VBO::SetBufferData(const std::vector<Vertex>& Vertices, BufferUsageHint UsageHint) const
{
	this->Bind();
	glBufferData(
		GL_ARRAY_BUFFER,
		Vertices.size() * sizeof(Vertex),
		Vertices.data(),
		UsageHint == BufferUsageHint::Dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW
	);
}

void VBO::Bind() const
{
	glBindBuffer(GL_ARRAY_BUFFER, this->ID);
}

void VBO::Unbind()
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

EBO::EBO()
{
	glGenBuffers(1, &this->ID);
	this->Bind();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, {}, GL_STREAM_DRAW);
}

EBO::~EBO()
{
	glDeleteBuffers(1, &this->ID);
}

void EBO::SetBufferData(const std::vector<GLuint>& Indices, BufferUsageHint UsageHint) const
{
	this->Bind();
	glBufferData(
		GL_ELEMENT_ARRAY_BUFFER,
		Indices.size() * sizeof(GLuint),
		Indices.data(),
		UsageHint == BufferUsageHint::Dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW
	);
}

void EBO::Bind() const
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ID);
}

void EBO::Unbind()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

FBO::FBO(int Width, int Height, int MSSamples, bool AttachRenderBuffer)
{
	this->MSAASamples = MSSamples;

	this->ID = 0;

	glGenFramebuffers(1, &this->ID);
	
	GLenum binding = MSSamples > 0 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

	glActiveTexture(GL_TEXTURE0);

	glGenTextures(1, &this->TextureID);
	glBindTexture(binding, this->TextureID);
	
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
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->TextureID, 0);

		glGenRenderbuffers(1, &this->RenderBufferID);
		glBindRenderbuffer(GL_RENDERBUFFER, this->RenderBufferID);

		if (MSSamples > 0)
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, MSSamples, GL_DEPTH24_STENCIL8, Width, Height);
		else
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, Width, Height);

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, this->RenderBufferID);
	}

	auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		throw(std::vformat("Could not create a framebuffer, error ID: {}", std::make_format_args(fboStatus)));
}

FBO::~FBO()
{
	glDeleteTextures(1, &this->TextureID);
	
	if (this->RenderBufferID)
		glDeleteRenderbuffers(1, &this->RenderBufferID);

	glDeleteFramebuffers(1, &this->ID);
}

void FBO::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, this->ID);
}

void FBO::Unbind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FBO::BindTexture() const
{
	glBindTexture(/*this->MSAASamples > 0 ? GL_TEXTURE_2D_MULTISAMPLE : */ GL_TEXTURE_2D, this->TextureID);
}

void FBO::UnbindTexture()
{
	glBindTexture(/*this->MSAASamples > 0 ? GL_TEXTURE_2D_MULTISAMPLE : */ GL_TEXTURE_2D, 0);
}
