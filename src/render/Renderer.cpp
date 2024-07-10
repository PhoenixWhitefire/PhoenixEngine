#include<string>
#include<format>
#include<glad/gl.h>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/transform.hpp>

#include"render/GraphicsAbstractionLayer.hpp"
#include"render/Renderer.hpp"
#include"datatype/Vector3.hpp"
#include"Debug.hpp"

GLenum ObjectTypes[] =
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

std::string DiffuseStr = "Diffuse";
std::string SpecularStr = "Specular";

void GLDebugCallback(
	GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
	// not very important
	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
		return;

	std::string SourceTmp;
	const char* Source;

	switch (source) {

	case GL_DEBUG_SOURCE_API:
	{
		Source = "OpenGL API";
		break;
	}

	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
	{
		Source = "Window system API";
		break;
	}

	case GL_DEBUG_SOURCE_SHADER_COMPILER:
	{
		Source = "Shader compiler";
		break;
	}

	case GL_DEBUG_SOURCE_THIRD_PARTY:
	{
		Source = "Third-party";
		break;
	}

	case GL_DEBUG_SOURCE_APPLICATION:
	{
		Source = "User-generated";
		break;
	}

	case GL_DEBUG_SOURCE_OTHER:
	{
		Source = "Other";
		break;
	}

	default:
	{
		SourceTmp = std::to_string(source);

		Source = SourceTmp.c_str();
		break;
	}

	}

	std::string SeverityTmp;
	const char* Severity;

	switch (severity) {

	case GL_DEBUG_SEVERITY_HIGH:
	{
		Severity = "HIGH";
		break;
	}

	case GL_DEBUG_SEVERITY_MEDIUM:
	{
		Severity = "Medium";
		break;
	}

	case GL_DEBUG_SEVERITY_LOW:
	{
		Severity = "Low";
		break;
	}

	case GL_DEBUG_SEVERITY_NOTIFICATION:
	{
		Severity = "Notification";
		break;
	}

	default:
	{
		SeverityTmp = std::to_string(severity);

		Severity = SeverityTmp.c_str();
		break;
	}

	}

	std::string TypeTmp;
	const char* Type;

	switch (type)
	{

	case GL_DEBUG_TYPE_ERROR: {
		Type = "ERROR";
		break;
	}

	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
	{
		Type = "Deprecated behaviour warning";
		break;
	}

	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
	{
		Type = "Undefined behaviour warning";
		break;
	}

	case GL_DEBUG_TYPE_PORTABILITY:
	{
		Type = "Not portable warning";
		break;
	}

	case GL_DEBUG_TYPE_PERFORMANCE:
	{
		Type = "Low-performance warning";
		break;
	}

	case GL_DEBUG_TYPE_MARKER:
	{
		Type = "Annotation";
		break;
	}

	case GL_DEBUG_TYPE_PUSH_GROUP:
	{
		Type = "Group push";
		break;
	}

	case GL_DEBUG_TYPE_POP_GROUP:
	{
		Type = "Group pop";
		break;
	}

	case GL_DEBUG_TYPE_OTHER:
	{
		Type = "Other";
		break;
	}

	default:
	{
		TypeTmp = std::to_string(type);

		Type = TypeTmp.c_str();
		break;
	}

	}

	const char* messageCChar = message;
	int idCInt = id;

	std::string DebugString = std::vformat(
		std::string("GL Debug callback:\n\tType: {}\n\tSeverity: {}\n\tMessage: {}\n\tSource: {}\n\tError ID: {}\n"),
		std::make_format_args(
			Type,
			Severity,
			message,
			Source,
			id
		)
	);

	Debug::Log(DebugString);

	if (type != GL_DEBUG_TYPE_PERFORMANCE)
		throw(DebugString);
}

Renderer::Renderer(uint32_t Width, uint32_t Height, SDL_Window* Window, uint8_t MSAASamples)
{
	this->m_window = Window;

	this->m_width = Width;
	this->m_height = Height;

	this->m_msaaSamples = MSAASamples;

	this->m_glContext = SDL_GL_CreateContext(this->m_window);

	std::string ErrorMessage = "NO ERRORS";

	if (!this->m_glContext)
	{
		const char* sdlErrStr = SDL_GetError();
		throw(std::vformat("Could not create an OpenGL context, SDL error: {}", std::make_format_args(sdlErrStr)));
	}

	int GLMakeCurCtxStatus = SDL_GL_MakeCurrent(this->m_window, this->m_glContext);

	if (GLMakeCurCtxStatus < 0)
		throw(std::vformat(
			"Could not set the current OpenGL context (SDL error): {}",
			std::make_format_args(GLMakeCurCtxStatus)
		));

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	bool GladStatus = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);

	if (!GladStatus)
		throw(std::string("GLAD could not load OpenGL. Please update your drivers (min OGL ver: 4.6)."));

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	glDebugMessageCallback(GLDebugCallback, 0);

	glEnable(GL_MULTISAMPLE);
	glEnable(GL_DEPTH_TEST);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CCW);
	
	SDL_GL_SetSwapInterval(1);

	glViewport(0, 0, this->m_width, this->m_height);

	int GLVersionMajor, GLVersionMinor = 0;

	glGetIntegerv(GL_MAJOR_VERSION, &GLVersionMajor);
	glGetIntegerv(GL_MINOR_VERSION, &GLVersionMinor);

	std::string glVersionConDbgStrTemp = std::vformat(
		"Running OpenGL version {}.{}",
		std::make_format_args(GLVersionMajor, GLVersionMinor)
	).c_str();
	const char* glVersionConDbgStr = glVersionConDbgStrTemp.c_str();

	Debug::Log(std::string(glVersionConDbgStr));

	this->m_vertexArray = new VAO();
	this->m_vertexArray->Bind();

	this->m_vertexBuffer = new VBO();
	this->m_elementBuffer = new EBO();

	this->m_vertexArray->LinkAttrib(*this->m_vertexBuffer, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0);
	this->m_vertexArray->LinkAttrib(*this->m_vertexBuffer, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
	this->m_vertexArray->LinkAttrib(*this->m_vertexBuffer, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
	this->m_vertexArray->LinkAttrib(*this->m_vertexBuffer, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));

	// TODO: batching draw calls
	// I actually coded this, then it didn't work, then I accidentally deleted the batching code when it was a 1-line fix
	// I am now sad

	// TODO: side effects of not doing this?
	/*
	this->m_vertexArray->Unbind();
	this->m_vertexBuffer->Unbind();
	this->m_elementBuffer->Unbind();
	*/

	/*this->m_framebuffer = new FBO(
		this->m_width,
		this->m_height,
		this->m_window,
		this->m_msaaSamples <= 0 ? false : true,
		this->m_msaaSamples
	 );*/

	this->m_framebuffer = new FBO(this->m_window, this->m_width, this->m_height, this->m_msaaSamples);

	Debug::Log("Renderer initialized");
}

void Renderer::ChangeResolution(uint32_t Width, uint32_t Height)
{
	std::string dbgWinResChangeStr = std::vformat(
		"Changing window resolution: ({}, {}) -> ({}, {})",
		std::make_format_args(
			this->m_width,
			this->m_height,
			Width,
			Height
		)
	);

	Debug::Log(dbgWinResChangeStr);

	this->m_width = Width;
	this->m_height = Height;

	glViewport(0, 0, this->m_width, this->m_height);

	// TODO fix msaa

	//this->m_framebuffer->Bind();
	this->m_framebuffer->BindTexture();

	if (this->m_framebuffer->MSAASamples > 0)
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, this->m_msaaSamples, GL_RGB, this->m_width, this->m_height, GL_TRUE);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, this->m_width, this->m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glBindRenderbuffer(GL_RENDERBUFFER, this->m_framebuffer->RenderBufferID);

	if (this->m_framebuffer->MSAASamples > 0)
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, this->m_msaaSamples, GL_DEPTH32F_STENCIL8, this->m_width, this->m_height);
	else
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, this->m_width, this->m_height);

	this->SwapBuffers();
}

void Renderer::DrawScene(Scene_t& Scene)
{
	uint32_t ShaderProgramID = Scene.Shaders->ID;

	Scene.Shaders->Activate();

	for (uint32_t LightIndex = 0; LightIndex < Scene.LightData.size(); LightIndex++)
	{
		LightData_t LightData = Scene.LightData.at(LightIndex);

		std::string LightIdxString = std::to_string(LightIndex);
		std::string ShaderLightLoc = "Lights[" + LightIdxString + "]";
		
		auto PosLoc = glGetUniformLocation(ShaderProgramID, (ShaderLightLoc + ".Position").c_str());
		auto ColLoc = glGetUniformLocation(ShaderProgramID, (ShaderLightLoc + ".Color").c_str());
		auto TypeLoc = glGetUniformLocation(ShaderProgramID, (ShaderLightLoc + ".Type").c_str());
		auto RangeLoc = glGetUniformLocation(ShaderProgramID, (ShaderLightLoc + ".Range").c_str());

		glUniform3f(PosLoc, LightData.Position.X, LightData.Position.Y, LightData.Position.Z);
		glUniform3f(ColLoc, LightData.LightColor.R, LightData.LightColor.G, LightData.LightColor.B);
		glUniform1i(TypeLoc, (int)LightData.Type);

		glUniform1f(RangeLoc, LightData.Range);

		glUniformMatrix4fv(
			glGetUniformLocation(ShaderProgramID, (ShaderLightLoc + ".ShadowMapProjection").c_str()),
			1,
			GL_FALSE,
			glm::value_ptr(LightData.ShadowMapProjection)
		);

		// TODO: replace true with LightData.HasShadowMap
		glUniform1i(glGetUniformLocation(ShaderProgramID, (ShaderLightLoc + ".HasShadowMap").c_str()), LightData.HasShadowMap);

		glUniform1i(glGetUniformLocation(ShaderProgramID, (ShaderLightLoc + ".ShadowMapIndex").c_str()), LightData.ShadowMapIndex);

		std::string ShadowMapLutKey = std::string("ShadowMaps[") + std::to_string(LightData.ShadowMapIndex) + "]";

		glUniform1i(
			glGetUniformLocation(
				ShaderProgramID,
				ShadowMapLutKey.c_str()
			),
			LightData.ShadowMapTextureId
		);
	}

	glUniform1i(glGetUniformLocation(ShaderProgramID, "NumLights"), Scene.LightData.size());

	for (uint32_t MeshIndex = 0; MeshIndex < Scene.MeshData.size(); MeshIndex++)
	{
		MeshData_t RenderData = Scene.MeshData.at(MeshIndex);
		this->m_setTextureUniforms(RenderData, Scene.Shaders);
		this->DrawMesh(RenderData.MeshData, Scene.Shaders, RenderData.Size, RenderData.Matrix, RenderData.FaceCulling);
	}

	for (uint32_t MeshIndex = 0; MeshIndex < Scene.TransparentMeshData.size(); MeshIndex++)
	{
		MeshData_t RenderData = Scene.TransparentMeshData.at(MeshIndex);
		this->m_setTextureUniforms(RenderData, Scene.Shaders);
		this->DrawMesh(RenderData.MeshData, Scene.Shaders, RenderData.Size, RenderData.Matrix, RenderData.FaceCulling);
	}
}

void Renderer::DrawMesh(
	Mesh* Object,
	ShaderProgram* Shaders,
	Vector3 Size,
	glm::mat4 Matrix,
	FaceCullingMode FaceCulling
)
{
	if (!this->m_glContext)
		throw(std::string("NULL m_glContext in DrawMesh!"));

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
		glCullFace(GL_FRONT); // WTF?
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

	this->m_vertexArray->Bind();

	this->m_vertexBuffer->SetBufferData(Object->Vertices);
	this->m_elementBuffer->SetBufferData(Object->Indices);

	glm::mat4 Scale = glm::mat4(1.0f);
	Scale = glm::scale(Scale, (glm::vec3)Size);

	glUniformMatrix4fv(glGetUniformLocation(Shaders->ID, "Matrix"), 1, GL_FALSE, glm::value_ptr(Matrix));
	glUniformMatrix4fv(glGetUniformLocation(Shaders->ID, "Scale"), 1, GL_FALSE, glm::value_ptr(Scale));

	glDrawElements(GL_TRIANGLES, Object->Indices.size(), GL_UNSIGNED_INT, 0);
}

void Renderer::m_setTextureUniforms(MeshData_t& RenderData, ShaderProgram* Shaders)
{
	RenderMaterial* Material = RenderData.Material;
	Shaders->Activate();

	int NumDiffuse = 0;
	int NumSpecular = 0;
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (RenderData.Transparency > 0.f)
		glEnable(GL_BLEND);
	else // the gosh darn grass model is practically 50% transparent
		glDisable(GL_BLEND);
	
	//glDisable(GL_BLEND);

	glUniform1f(glGetUniformLocation(Shaders->ID, "SpecularMultiplier"), Material->SpecMultiply);
	glUniform1f(glGetUniformLocation(Shaders->ID, "SpecularPower"), Material->SpecExponent);

	glUniform1f(glGetUniformLocation(Shaders->ID, "Reflectivity"), RenderData.Reflectivity);
	glUniform1f(glGetUniformLocation(Shaders->ID, "Transparency"), RenderData.Transparency);

	glUniform3f(glGetUniformLocation(Shaders->ID, "ColorTint"),
		RenderData.TintColor.R,
		RenderData.TintColor.G,
		RenderData.TintColor.B
	);

	//glUniform1i(glGetUniformLocation(Shaders->ID, "HasSpecular"), 0);
	

	/*glUniform1i(glGetUniformLocation(Shaders->ID, "HasDiffuse"), 1);

	glActiveTexture(GL_TEXTURE0);

	glBindTexture(GL_TEXTURE_2D, Material->DiffuseTextures[0]->Identifier);

	glUniform1i(glGetUniformLocation(Shaders->ID, "DiffuseTextures[0]"), 0);

	glUniform1i(glGetUniformLocation(Shaders->ID, "ActiveDiffuseTextures"), 1);

	return;*/

	uint32_t TexUnitOffset = 5;

	for (Texture* Image : Material->DiffuseTextures)
	{
		std::string NumberOf;
		std::string Type;

		TexUnitOffset++;

		Type = DiffuseStr;

		NumberOf = std::to_string(NumDiffuse);

		if (NumDiffuse > 6)
		{
			Debug::Log("DIFFUSE TEX LIMIT REACHED!");
			continue;
		}

		glActiveTexture(GL_TEXTURE0 + TexUnitOffset);

		Shaders->Activate();

		glBindTexture(GL_TEXTURE_2D, Image->Identifier);

		std::string ShaderTextureVar = (Type + "Textures[0]");
		glUniform1i(glGetUniformLocation(Shaders->ID, ShaderTextureVar.c_str()), TexUnitOffset);
	}

	for (Texture* Image : Material->SpecularTextures)
	{
		std::string NumberOf;
		std::string Type;

		TexUnitOffset++;

		Type = SpecularStr;

		NumberOf = std::to_string(NumSpecular);

		if (NumSpecular > 6)
		{
			Debug::Log("SPECULAR TEX LIMIT REACHED!");
			continue;
		}

		glActiveTexture(GL_TEXTURE0 + TexUnitOffset);

		Shaders->Activate();

		glBindTexture(GL_TEXTURE_2D, Image->Identifier);

		std::string ShaderTextureVar = (Type + "Textures[0]");
		glUniform1i(glGetUniformLocation(Shaders->ID, ShaderTextureVar.c_str()), TexUnitOffset);
	}

	glUniform1i(glGetUniformLocation(Shaders->ID, "NumDiffuseTextures"), Material->DiffuseTextures.size());
	glUniform1i(glGetUniformLocation(Shaders->ID, "NumSpecularTextures"), Material->SpecularTextures.size());
}

void Renderer::SwapBuffers()
{
	SDL_GL_SwapWindow(this->m_window);
}
