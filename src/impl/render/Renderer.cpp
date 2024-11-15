#include <string>
#include <format>
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <SDL2/SDL_video.h>

#include "render/Renderer.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/MeshProvider.hpp"
#include "GlobalJsonConfig.hpp"
#include "Debug.hpp"

constexpr uint32_t SHADER_MAX_LIGHTS = 6;

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
	
	glViewport(0, 0, m_Width, m_Height);

	int glVersionMajor, glVersionMinor = 0;

	glGetIntegerv(GL_MAJOR_VERSION, &glVersionMajor);
	glGetIntegerv(GL_MINOR_VERSION, &glVersionMinor);

	std::string glVersionStr = std::vformat(
		"Running OpenGL version {}.{}",
		std::make_format_args(glVersionMajor, glVersionMinor)
	);

	Debug::Log(glVersionStr);

	m_VertexArray = new GpuVertexArray;
	m_VertexBuffer = new GpuVertexBuffer;
	m_ElementBuffer = new GpuElementBuffer;

	m_VertexArray->Bind();

	m_VertexArray->LinkAttrib(*m_VertexBuffer, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0);
	m_VertexArray->LinkAttrib(*m_VertexBuffer, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
	m_VertexArray->LinkAttrib(*m_VertexBuffer, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
	m_VertexArray->LinkAttrib(*m_VertexBuffer, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));

	this->Framebuffer = new GpuFrameBuffer(m_Width, m_Height, m_MsaaSamples);

	glGenBuffers(1, &m_InstancingBuffer);

	Debug::Log("Renderer initialized");
}

Renderer::~Renderer()
{
	delete m_VertexArray;
	delete m_VertexBuffer;
	delete m_ElementBuffer;
	glDeleteBuffers(1, &m_InstancingBuffer);
	delete this->Framebuffer;

	m_VertexArray = nullptr;
	m_VertexBuffer = nullptr;
	m_ElementBuffer = nullptr;
	this->Framebuffer = nullptr;

	this->GLContext = nullptr;
	m_Window = nullptr;
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

	this->Framebuffer->UpdateResolution(m_Width, m_Height);
}

void Renderer::DrawScene(
	const Scene& Scene,
	const glm::mat4& RenderMatrix,
	const glm::mat4& CameraTransform,
	double RunningTime
)
{
	glActiveTexture(GL_TEXTURE0);
	this->Framebuffer->BindTexture();

	ShaderManager* shdManager = ShaderManager::Get();

	for (uint32_t shaderId : Scene.UsedShaders)
	{
		ShaderProgram& shader = shdManager->GetShaderResource(shaderId);

		shader.SetUniform("RenderMatrix", RenderMatrix);
		shader.SetUniform("CameraPosition", Vector3(glm::vec3(CameraTransform[3])).ToGenericValue());
		shader.SetUniform("Time", static_cast<float>(RunningTime));
		shader.SetUniform("SkyboxCubemap", 3);

		shader.SetUniform("FrameBuffer", 0);

		// TODO 05/09/2024
		// Branching in shader VS separate array uniforms?
		// Oh and uniform locations should probably be cached
		for (size_t lightIndex = 0; lightIndex < SHADER_MAX_LIGHTS; lightIndex++)
		{
			if (lightIndex + 1 > Scene.LightingList.size())
				break;

			const LightItem& lightData = Scene.LightingList.at(lightIndex);

			std::string lightIdxString = std::to_string(lightIndex);
			std::string shaderLightLoc = "Lights[" + lightIdxString + "]";

			shader.SetUniform(
				(shaderLightLoc + ".Position").c_str(),
				lightData.Position.ToGenericValue()
			);

			shader.SetUniform(
				(shaderLightLoc + ".Color").c_str(),
				lightData.LightColor.ToGenericValue()
			);

			shader.SetUniform((shaderLightLoc + ".Type").c_str(), (int)lightData.Type);

			shader.SetUniform((shaderLightLoc + ".Range").c_str(), lightData.Range);
		}

		shader.SetUniform(
			"NumLights",
			std::clamp(
				static_cast<uint32_t>(Scene.LightingList.size()),
				0u,
				SHADER_MAX_LIGHTS
			)
		);
	}

	MeshProvider* meshProvider = MeshProvider::Get();
	MaterialManager* mtlManager = MaterialManager::Get();

	size_t numDrawCalls = 0;

	static const bool DoInstancing = true;

	// map< instance checksum, pair< base RenderItem, vector< array buffer data >>>
	std::map<uint64_t, std::pair<size_t, std::vector<float>>> instancingList;

	for (size_t renderItemIndex = 0; renderItemIndex < Scene.RenderList.size(); renderItemIndex++)
	{
		const RenderItem& renderData = Scene.RenderList[renderItemIndex];

		if (DoInstancing)
		{
			// the MESH, MATERIAL, TRANSPARENCY and REFLECTIVITY must be the same
			// for a set of objects to be instanced together
			// And it needs to be on the GPU
			Mesh& mesh = meshProvider->GetMeshResource(renderData.RenderMeshId);

			uint64_t checksum = 0;

			if (mesh.GpuId == UINT32_MAX)
			{
				// make sure meshes that aren't on the gpu don't get
				// instanced
				checksum = instancingList.size();

				while (instancingList.find(checksum) != instancingList.end())
					checksum += 1;
			}
			else
				checksum = renderData.RenderMeshId
					+ static_cast<uint64_t>(renderData.MaterialId * 500u)
					+ static_cast<uint64_t>(renderData.Transparency * 250)
					+ static_cast<uint64_t>(renderData.Reflectivity * 115);

			// 14/11/2024
			// disabled for now because it causes a bunch of weird flickering
			/*
			if (renderData.Transparency > 0.f)
				// hacky way to get transparents closer to the camera drawn
				// later.
				checksum += static_cast<uint64_t>(1.f / (glm::distance(
					CameraTransform[3],
					renderData.Transform[3]
				)) * 5000.f);
			*/

			auto it = instancingList.find(checksum);
			if (it == instancingList.end())
				instancingList[checksum] = std::pair(renderItemIndex, std::vector<float>{});

			// Set buffer data
			// Transform
			for (int8_t col = 0; col < 4; col++)
				for (int8_t row = 0; row < 4; row++)
					instancingList[checksum].second.push_back(renderData.Transform[col][row]);

			// Size
			instancingList[checksum].second.push_back(renderData.Size.x);
			instancingList[checksum].second.push_back(renderData.Size.y);
			instancingList[checksum].second.push_back(renderData.Size.z);

			// Color
			instancingList[checksum].second.push_back(renderData.TintColor.R);
			instancingList[checksum].second.push_back(renderData.TintColor.G);
			instancingList[checksum].second.push_back(renderData.TintColor.B);
		}
		else
		{
			m_SetMaterialData(renderData);

			this->DrawMesh(
				meshProvider->GetMeshResource(renderData.RenderMeshId),
				mtlManager->GetMaterialResource(renderData.MaterialId).GetShader(),
				renderData.Size,
				renderData.Transform,
				renderData.FaceCulling,
				0
			);

			numDrawCalls++;
		}
	}

	if (DoInstancing)
	{
		for (auto& iter : instancingList)
		{
			const RenderItem& renderData = Scene.RenderList[iter.second.first];
			const Mesh& mesh = meshProvider->GetMeshResource(renderData.RenderMeshId);
			const std::vector<float>& drawInfos = iter.second.second;

			if (mesh.GpuId != UINT32_MAX)
			{
				MeshProvider::GpuMesh& gpuMesh = meshProvider->GetGpuMesh(mesh.GpuId);
				gpuMesh.VertexArray->Bind();

				glBindBuffer(GL_ARRAY_BUFFER, m_InstancingBuffer);

				constexpr int32_t vec4Size = static_cast<int32_t>(sizeof(glm::vec4));
				constexpr int32_t vec3Size = static_cast<int32_t>(sizeof(glm::vec3));

				// TODO 14/11/2024
				// I don't like having these here, but it looks like both the Vertex Array
				// and the Instancing Buffer need to be bound for it to function

				// `Transform` matrix
				// 4 vec4's
				glEnableVertexAttribArray(4);
				glEnableVertexAttribArray(5);
				glEnableVertexAttribArray(6);
				glEnableVertexAttribArray(7);

				glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size + vec3Size*2, (void*)0);
				glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size + vec3Size*2, (void*)(1 * (size_t)vec4Size));
				glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size + vec3Size*2, (void*)(2 * (size_t)vec4Size));
				glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size + vec3Size*2, (void*)(3 * (size_t)vec4Size));

				glVertexAttribDivisor(4, 1);
				glVertexAttribDivisor(5, 1);
				glVertexAttribDivisor(6, 1);
				glVertexAttribDivisor(7, 1);

				// vec3s
				// scale
				glEnableVertexAttribArray(8);
				// color
				glEnableVertexAttribArray(9);

				// it's still `4 * (size_t)` because that's the offset from everything else
				glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, 4 * vec4Size + vec3Size*2, (void*)(4 * (size_t)vec4Size));
				glVertexAttribPointer(9, 3, GL_FLOAT, GL_FALSE, 4 * vec4Size + vec3Size*2, (void*)((4 * (size_t)vec4Size) + vec3Size));

				glVertexAttribDivisor(8, 1);
				glVertexAttribDivisor(9, 1);

				glBufferData(
					GL_ARRAY_BUFFER,
					drawInfos.size() * sizeof(float),
					drawInfos.data(),
					GL_STREAM_DRAW
				);
			}
			else
				glBindBuffer(GL_ARRAY_BUFFER, 0);

			RenderMaterial& material = mtlManager->GetMaterialResource(renderData.MaterialId);

			m_SetMaterialData(renderData);

			constexpr size_t ElementsPerInstance = 22ull;

			if (drawInfos.size() % ElementsPerInstance != 0)
				throw("`drawInfos` was not divisible by `ElementsPerInstance`");

			int32_t numInstances = static_cast<int32_t>(drawInfos.size() / ElementsPerInstance);

			this->DrawMesh(
				mesh,
				material.GetShader(),
				renderData.Size,
				renderData.Transform,
				renderData.FaceCulling,
				numInstances > 1 ? numInstances : 0
			);

			numDrawCalls++;
		}
	}

	EngineJsonConfig["renderer_drawcallcount"] = numDrawCalls;
}

void Renderer::DrawMesh(
	const Mesh& Object,
	ShaderProgram& Shader,
	const Vector3& Size,
	const glm::mat4& Transform,
	FaceCullingMode FaceCulling,
	int32_t NumInstances
)
{
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

	uint32_t gpuMeshId = Object.GpuId;
	MeshProvider::GpuMesh* gpuMesh = nullptr;

	// mesh not uploaded to the GPU by MeshProvider
	if (gpuMeshId == UINT32_MAX)
	{
		m_VertexArray->Bind();

		m_VertexBuffer->SetBufferData(Object.Vertices);
		m_ElementBuffer->SetBufferData(Object.Indices);

		m_VertexBuffer->Bind();
		m_ElementBuffer->Bind();
	}
	else
	{
		gpuMesh = &MeshProvider::Get()->GetGpuMesh(gpuMeshId);
		gpuMesh->VertexArray->Bind();
		gpuMesh->VertexBuffer->Bind();
		gpuMesh->ElementBuffer->Bind();
	}

	uint32_t numIndices = gpuMesh ? gpuMesh->NumIndices : static_cast<uint32_t>(Object.Indices.size());

	if (NumInstances > 0)
	{
		Shader.SetUniform("IsInstanced", true);
		Shader.Activate();

		glDrawElementsInstanced(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0, NumInstances);
	}
	else
	{
		Shader.SetUniform("IsInstanced", false);
		Shader.SetUniform("Transform", Transform);
		Shader.SetUniform("Scale", Size.ToGenericValue());
		Shader.Activate();

		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
	}
}

void Renderer::m_SetMaterialData(const RenderItem& RenderData)
{
	MaterialManager* mtlManager = MaterialManager::Get();
	TextureManager* texManager = TextureManager::Get();

	RenderMaterial& material = mtlManager->GetMaterialResource(RenderData.MaterialId);
	ShaderProgram& shader = material.GetShader();

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (RenderData.Transparency > 0.f || material.HasTranslucency)
		glEnable(GL_BLEND);
	else // the gosh darn grass model is practically 50% transparent
		glDisable(GL_BLEND);
	
	shader.SetUniform("SpecularMultiplier", material.SpecMultiply);
	shader.SetUniform("SpecularPower", material.SpecExponent);

	shader.SetUniform("Reflectivity", RenderData.Reflectivity);
	shader.SetUniform("Transparency", RenderData.Transparency);

	shader.SetUniform(
		"ColorTint",
		RenderData.TintColor.ToGenericValue()
	);

	/*
		TODO 05/09/2024:

		I'm conflicted whether to allow multiple textures of the same type

		PROS:
		* Texture "masking"
		* Multiple UV map support
		
		CONS:
		* Extra machinery and boilerplate, and it's overall more effort
	*/

	Texture& colorMap = texManager->GetTextureResource(material.ColorMap);
	Texture& metallicRoughnessMap = texManager->GetTextureResource(material.MetallicRoughnessMap);
	Texture& normalMap = texManager->GetTextureResource(material.NormalMap);

	shader.SetTextureUniform("ColorMap", colorMap.GpuId);
	shader.SetTextureUniform("MetallicRoughnessMap", metallicRoughnessMap.GpuId);

	if (material.NormalMap != 0)
	{
		shader.SetUniform("HasNormalMap", true);
		shader.SetTextureUniform("NormalMap", normalMap.GpuId);
	}
	else
		shader.SetUniform("HasNormalMap", false);

	// apply the uniforms for the shader program...
	shader.ApplyDefaultUniforms();
	// ... then the material uniforms...
	material.ApplyUniforms();
	// ... so that the material can override uniforms in the SP
}

void Renderer::SwapBuffers()
{
	SDL_GL_SwapWindow(m_Window);
}
