#include<string>
#include<format>
#include<glad/gl.h>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/transform.hpp>
#include<microprofile/microprofile.h>

#include"render/GraphicsAbstractionLayer.hpp"
#include"render/Renderer.hpp"
#include"datatype/Vector3.hpp"
#include"Debug.hpp"

static std::string const& DiffuseStr = "Diffuse";
static std::string const& SpecularStr = "Specular";

static GLenum ObjectTypes[] =
{
	GL_BUFFER,
	GL_SHADER,
	GL_PROGRAM,
	GL_VERTEX_ARRAY,
	GL_QUERY,
	GL_PROGRAM_PIPELINE,
//	GL_TRANSFORM_FEEDBACK,
	GL_SAMPLER,
	GL_TEXTURE,
	GL_RENDERBUFFER,
	GL_FRAMEBUFFER
};

static std::unordered_map<GLenum, std::string> GLEnumToStringMap =
{
	{ GL_DEBUG_SOURCE_API, "OpenGL"},
	{ GL_DEBUG_SOURCE_WINDOW_SYSTEM, "Window system" },
	{ GL_DEBUG_SOURCE_SHADER_COMPILER, "Shader compiler" },
	{ GL_DEBUG_SOURCE_THIRD_PARTY, "Third-party" },
	{ GL_DEBUG_SOURCE_APPLICATION, "User-generated :3" },
	{ GL_DEBUG_SOURCE_OTHER, "Other" },

	{ GL_DEBUG_SEVERITY_HIGH, "High" },
	{ GL_DEBUG_SEVERITY_MEDIUM, "Medium" },
	{ GL_DEBUG_SEVERITY_LOW, "Low" },
	{ GL_DEBUG_SEVERITY_NOTIFICATION, "Notification" },

	{ GL_DEBUG_TYPE_ERROR, "Error" },
	{ GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "Deprecated behavior" },
	{ GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, "Undefined behavior" },
	{ GL_DEBUG_TYPE_PORTABILITY, "Portability" },
	{ GL_DEBUG_TYPE_PERFORMANCE, "Performance" },
	{ GL_DEBUG_TYPE_MARKER, "Marker" },
	{ GL_DEBUG_TYPE_PUSH_GROUP, "Push group" },
	{ GL_DEBUG_TYPE_POP_GROUP, "Pop group" },
	{ GL_DEBUG_TYPE_OTHER, "Other" }
};

static std::string glEnumToString(GLenum Id)
{
	auto it = GLEnumToStringMap.find(Id);
	if (it != GLEnumToStringMap.end())
		return it->second;
	else
		return std::vformat("ID:{}", std::make_format_args(Id));
}

static void GLDebugCallback(
	GLenum SourceId,
	GLenum TypeId,
	GLuint Id,
	GLenum SeverityId,
	GLsizei MessageLength,
	const GLchar* Message,
	const void* /*Userparam*/
)
{
	// not very important
	//if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
		//return;

	// ID for:
	// "Buffer object X will use VIDEO memory as the source for buffer object operations"
	if (Id == 131185)
		return;

	std::string sourceName = glEnumToString(SourceId);
	std::string severityName = glEnumToString(SeverityId);
	std::string typeName = glEnumToString(TypeId);

	std::string messageStr = Message;

	// `GLsizei` is a smelly `int`
	int actualMessageLength = static_cast<int>(strlen(Message));

	// It feels important that a `MessageLength` is provided,
	// maybe some drivers do not NULL-terminate their strings correctly?
	if (actualMessageLength != MessageLength)
	{
		Debug::Log(std::vformat(
			"Renderer::GLDebugCallback: `strlen` differs from message length: {} vs {}",
			std::make_format_args(actualMessageLength, MessageLength)
		));

		messageStr = messageStr.substr(0, MessageLength);
	}

	std::string debugString = std::vformat(
		std::string("GL Debug callback:\n\tType: {}\n\tSeverity: {}\n\tMessage: {}\n\tSource: {}\n\tError ID: {}\n"),
		std::make_format_args(
			typeName,
			severityName,
			messageStr,
			sourceName,
			Id
		)
	);

	Debug::Log(debugString);

	// ID 131218:
	// "Vertex shader in program is being recompiled based on GL state"
	if (Id != 131218 && SeverityId > GL_DEBUG_SEVERITY_NOTIFICATION)
		throw(debugString);
}

Renderer::Renderer(uint32_t Width, uint32_t Height, SDL_Window* Window)
{
	m_Window = Window;

	m_Width = Width;
	m_Height = Height;

	this->GLContext = SDL_GL_CreateContext(m_Window);

	if (!this->GLContext)
	{
		const char* sdlErrStr = SDL_GetError();
		throw(std::vformat("Could not create an OpenGL context, SDL error: {}", std::make_format_args(sdlErrStr)));
	}

	PHX_SDL_CALL(SDL_GL_MakeCurrent, m_Window, this->GLContext);

	bool gladStatus = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);

	if (!gladStatus)
		throw("GLAD could not load OpenGL. Please update your drivers.");

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	// `glDebugMessageCallback` will be NULL if the user
	// does not have the `GL_ARB_debug_output`/`GL_KHR_debug` OpenGL extensions
	// I just want this to work on a specific machine
	// 13/09/2024
	if (glDebugMessageCallback)
		glDebugMessageCallback(GLDebugCallback, nullptr);
	
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_DEPTH_TEST);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CCW);
	
	glViewport(0, 0, m_Width, m_Height);

	int glVersionMajor, glVersionMinor = 0;

	glGetIntegerv(GL_MAJOR_VERSION, &glVersionMajor);
	glGetIntegerv(GL_MINOR_VERSION, &glVersionMinor);

	std::string glVersionStr = std::vformat(
		"Running OpenGL version {}.{}",
		std::make_format_args(glVersionMajor, glVersionMinor)
	).c_str();

	Debug::Log(glVersionStr);

	m_VertexArray = new VAO();
	m_VertexArray->Bind();

	m_VertexBuffer = new VBO();
	m_ElementBuffer = new EBO();

	m_VertexArray->LinkAttrib(*m_VertexBuffer, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0);
	m_VertexArray->LinkAttrib(*m_VertexBuffer, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
	m_VertexArray->LinkAttrib(*m_VertexBuffer, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
	m_VertexArray->LinkAttrib(*m_VertexBuffer, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));

	this->Framebuffer = new FBO(m_Width, m_Height, m_MsaaSamples);

	Debug::Log("Renderer initialized");
}

void Renderer::ChangeResolution(uint32_t Width, uint32_t Height)
{
	std::string resChangedStr = std::vformat(
		"Changing window resolution: ({}, {}) -> ({}, {})",
		std::make_format_args(
			m_Width,
			m_Height,
			Width,
			Height
		)
	);

	Debug::Log(resChangedStr);

	m_Width = Width;
	m_Height = Height;

	glViewport(0, 0, m_Width, m_Height);

	// TODO fix msaa

	//this->m_framebuffer->Bind();
	this->Framebuffer->BindTexture();

	if (this->Framebuffer->MSAASamples > 0)
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_MsaaSamples, GL_RGB, m_Width, m_Height, GL_TRUE);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_Width, m_Height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glBindRenderbuffer(GL_RENDERBUFFER, this->Framebuffer->RenderBufferID);

	if (this->Framebuffer->MSAASamples > 0)
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_MsaaSamples, GL_DEPTH32F_STENCIL8, m_Width, m_Height);
	else
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, m_Width, m_Height);
}

void Renderer::DrawScene(const Scene& Scene)
{
	MICROPROFILE_SCOPEI("Rendering", "Draw Scene", MP_YELLOW);

	{
		MICROPROFILE_SCOPEI("Rendering", "Set lighting data", MP_YELLOW);

		for (ShaderProgram* shader : Scene.UniqueShaders)
		{
			// TODO 05/09/2024
			// Branching in shader VS separate array uniforms?
			// Oh and uniform locations should probably be cached
			for (uint32_t lightIndex = 0; lightIndex < Scene.LightingList.size(); lightIndex++)
			{
				LightItem lightData = Scene.LightingList.at(lightIndex);

				std::string lightIdxString = std::to_string(lightIndex);
				std::string shaderLightLoc = "Lights[" + lightIdxString + "]";

				shader->SetUniformFloat3(
					(shaderLightLoc + ".Position").c_str(),
					static_cast<float>(lightData.Position.X),
					static_cast<float>(lightData.Position.Y),
					static_cast<float>(lightData.Position.Z)
				);

				shader->SetUniformFloat3(
					(shaderLightLoc + ".Color").c_str(),
					static_cast<float>(lightData.LightColor.R),
					static_cast<float>(lightData.LightColor.G),
					static_cast<float>(lightData.LightColor.B)
				);

				shader->SetUniformInt((shaderLightLoc + ".Type").c_str(), (int)lightData.Type);

				shader->SetUniformFloat((shaderLightLoc + ".Range").c_str(), lightData.Range);
			}

			shader->SetUniformInt("NumLights", static_cast<int32_t>(Scene.LightingList.size()));
		}
	}

	for (RenderItem RenderData : Scene.RenderList)
	{
		m_SetMaterialData(RenderData, RenderData.Material->Shader);

		this->DrawMesh(
			RenderData.RenderMesh,
			RenderData.Material->Shader,
			RenderData.Size,
			RenderData.Transform,
			RenderData.FaceCulling
		);
	}
}

void Renderer::DrawMesh(
	Mesh* Object,
	ShaderProgram* Shaders,
	Vector3 Size,
	glm::mat4 Transform,
	FaceCullingMode FaceCulling
)
{
	MICROPROFILE_SCOPEI("Rendering", "Draw Mesh", MP_YELLOW);

	switch (FaceCulling)
	{

	case FaceCullingMode::None:
	{
		glDisable(GL_CULL_FACE);
		break;
	}

	case FaceCullingMode::BackFace:
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		break;
	}

	case FaceCullingMode::FrontFace:
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		break;
	}

	}

	Shaders->Activate();

	m_VertexArray->Bind();

	m_VertexBuffer->SetBufferData(Object->Vertices);
	m_ElementBuffer->SetBufferData(Object->Indices);

	glm::mat4 scale = glm::scale(glm::mat4(1.f), glm::vec3(Size));

	Shaders->SetUniformMatrix("Matrix", Transform);
	Shaders->SetUniformMatrix("Scale", scale);

	glDrawElements(GL_TRIANGLES, static_cast<int>(Object->Indices.size()), GL_UNSIGNED_INT, 0);
}

void Renderer::m_SetMaterialData(const RenderItem& RenderData, ShaderProgram* Shader)
{
	MICROPROFILE_SCOPEI("Rendering", "Set material data", MP_YELLOW);

	RenderMaterial* material = RenderData.Material;
	Shader->Activate();

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (RenderData.Transparency > 0.f || RenderData.Material->HasTranslucency)
		glEnable(GL_BLEND);
	else // the gosh darn grass model is practically 50% transparent
		glDisable(GL_BLEND);
	
	Shader->SetUniformFloat("SpecularMultiplier", material->SpecMultiply);
	Shader->SetUniformFloat("SpecularPower", material->SpecExponent);

	Shader->SetUniformFloat("Reflectivity", RenderData.Reflectivity);
	Shader->SetUniformFloat("Transparency", RenderData.Transparency);

	Shader->SetUniformFloat3(
		"ColorTint",
		RenderData.TintColor.R,
		RenderData.TintColor.G,
		RenderData.TintColor.B
	);

	TextureManager* texManager = TextureManager::Get();

	/*
		TODO 05/09/2024:

		I'm conflicted whether to allow multiple textures of the same type

		PROS:
		* Texture "masking"
		* Multiple UV map support
		
		CONS:
		* Extra machinery and boilerplate, and it's overall more effort
	*/

	Texture* colorMap = texManager->GetTextureResource(material->ColorMap);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, colorMap->GpuId);
	
	Shader->SetUniformInt((DiffuseStr + "Textures[0]").c_str(), 4);

	Texture* metallicRoughnessMap = texManager->GetTextureResource(material->MetallicRoughnessMap);

	if (metallicRoughnessMap)
	{
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, metallicRoughnessMap->GpuId);

		Shader->SetUniformInt((SpecularStr + "Textures[0]").c_str(), 5);
	}

	Shader->SetUniformInt("NumDiffuseTextures", 1);
	Shader->SetUniformInt("NumSpecularTextures", metallicRoughnessMap ? 1 : 0);
}

void Renderer::SwapBuffers()
{
	SDL_GL_SwapWindow(m_Window);
}
