#ifndef __GNUC__

#define EXPORT_SYMBOL __declspec(dllexport)

#else

#define EXPORT_SYMBOL __attribute__((visibility("default")))

#endif

extern "C"
{
	// 25/12/2024
	// request discrete GPU for NVIDIA and AMD respectively
	// (i'm so inclusive, even though i have an NV card i added the flag for AMD)

	EXPORT_SYMBOL unsigned long NvOptimusEnablement = 0x00000001;
	EXPORT_SYMBOL int AmdPowerXpressRequestHighPerformance = 1;
}

#define GLM_ENABLE_EXPERIMENTAL

#include <string>
#include <format>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include <SDL3/SDL_video.h>

#include <tracy/Tracy.hpp>

#include <glad/gl.h>

#include "render/Renderer.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/MeshProvider.hpp"
#include "GlobalJsonConfig.hpp"
#include "Timing.hpp"
#include "Log.hpp"

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

const char* LightLocs[8] = { 0 };
const char* LightPosLocs[8] = { 0 };
const char* LightColLocs[8] = { 0 };
const char* LightTypeLocs[8] = { 0 };
const char* LightRangeLocs[8] = { 0 };
const char* LightAngLocs[8] = { 0 };
const char* LightShadowsLocs[8] = { 0 };

static std::string glEnumToString(GLenum Id)
{
	auto it = GLEnumToStringMap.find(Id);
	if (it != GLEnumToStringMap.end())
		return it->second;
	else
		return std::format("ID:{}", Id);
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
	
	std::string debugString = std::format(
		"GL Debug callback:\n\tType: {}\n\tSeverity: {}\n\tMessage: {}\n\tSource: {}\n\tError ID: {}\n",
		glEnumToString(TypeId),
		glEnumToString(SeverityId),
		std::string_view(Message, MessageLength),
		glEnumToString(SourceId),
		Id
	);

	Log::Warning(debugString);

	// ID 131218:
	// "Vertex shader in program is being recompiled based on GL state"
	if (Id != 131218 && SeverityId > GL_DEBUG_SEVERITY_NOTIFICATION)
		if (EngineJsonConfig.value("render_glerrorsarefatal", true))
			RAISE_RT(debugString);
}

static Renderer* s_Instance = nullptr;

Renderer::Renderer(uint32_t Width, uint32_t Height, SDL_Window* Window)
{
	this->Initialize(Width, Height, Window);
}

void Renderer::Initialize(uint32_t Width, uint32_t Height, SDL_Window* Window)
{
	m_Window = Window;

	m_Width = Width;
	m_Height = Height;

	this->GLContext = SDL_GL_CreateContext(m_Window);

	if (!this->GLContext)
		RAISE_RT(std::format("Could not create an OpenGL context, SDL error: {}", SDL_GetError()));

	PHX_SDL_CALL(SDL_GL_MakeCurrent, m_Window, this->GLContext);

	bool gladStatus = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);

	if (!gladStatus)
		RAISE_RT("GLAD could not load OpenGL. Please update your drivers.");

	// `glDebugMessageCallback` will be NULL if the user
	// does not have the `GL_ARB_debug_output`/`GL_KHR_debug` OpenGL extensions
	// I just want this to work on a specific machine
	// 13/09/2024
	if (glDebugMessageCallback)
	{
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

		glDebugMessageCallback(GLDebugCallback, nullptr);
	}
	else
		Log::Warning("No `glDebugMessageCallback`");

	glEnable(GL_MULTISAMPLE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	
	glViewport(0, 0, m_Width, m_Height);

	int glVersionMajor, glVersionMinor = 0;
	int maxVertexAttribs = 0;

	glGetIntegerv(GL_MAJOR_VERSION, &glVersionMajor);
	glGetIntegerv(GL_MINOR_VERSION, &glVersionMinor);
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);

	Log::Info(std::format(
		"Running OpenGL version {}.{}",
		glVersionMajor, glVersionMinor
	));
	Log::Info(std::format("Max vertex attribs: {}", maxVertexAttribs));

	m_VertexArray.Initialize();
	m_VertexBuffer.Initialize();
	m_ElementBuffer.Initialize();

	m_VertexArray.Bind();

	m_VertexArray.LinkAttrib(m_VertexBuffer, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0);
	m_VertexArray.LinkAttrib(m_VertexBuffer, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
	m_VertexArray.LinkAttrib(m_VertexBuffer, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
	m_VertexArray.LinkAttrib(m_VertexBuffer, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));

	this->FrameBuffer.Initialize(m_Width, m_Height, m_MsaaSamples);

	glGenBuffers(1, &InstancingBuffer);

#define SETLIGHTLOCS(i) LightLocs[i] = "Lights[" #i "]"; \
LightPosLocs[i] = "Lights[" #i "].Position";             \
LightColLocs[i] = "Lights[" #i "].Color";                \
LightTypeLocs[i] = "Lights[" #i "].Type";                \
LightRangeLocs[i] = "Lights[" #i "].Range";              \
LightAngLocs[i] = "Lights[" #i "].Angle";                \
LightShadowsLocs[i] = "Lights[" #i "].Shadows";          \

	SETLIGHTLOCS(0);
	SETLIGHTLOCS(1);
	SETLIGHTLOCS(2);
	SETLIGHTLOCS(3);
	SETLIGHTLOCS(4);
	SETLIGHTLOCS(5);
	SETLIGHTLOCS(6);
	SETLIGHTLOCS(7);

#undef SETLIGHTLOCS

	assert(!s_Instance);
	s_Instance = this;

	Log::Info("Renderer initialized");
}

Renderer* Renderer::Get()
{
	assert(s_Instance);
	return s_Instance;
}

Renderer::~Renderer()
{
	s_Instance = nullptr;

	glDeleteBuffers(1, &InstancingBuffer);

	m_VertexArray.Delete();
	m_ElementBuffer.Delete();
	m_VertexBuffer.Delete();
	this->FrameBuffer.Delete();

	this->GLContext = nullptr;
	m_Window = nullptr;
}

void Renderer::ChangeResolution(uint32_t Width, uint32_t Height)
{
	Log::Info(std::format(
		"Changing window resolution: ({}, {}) -> ({}, {})",
		m_Width,
		m_Height,
		Width,
		Height
	));

	m_Width = Width;
	m_Height = Height;

	glViewport(0, 0, m_Width, m_Height);

	this->FrameBuffer.ChangeResolution(m_Width, m_Height);
}

void Renderer::DrawScene(
	const Scene& Scene,
	const glm::mat4& RenderMatrix,
	const glm::mat4& CameraTransform,
	double RunningTime,
	bool DebugWireframeRendering
)
{
	TIME_SCOPE_AS("DrawScene");
	ZoneScopedC(tracy::Color::HotPink);

	MeshProvider* meshProvider = MeshProvider::Get();
	MaterialManager* mtlManager = MaterialManager::Get();

	// map< clump hash, pair< base RenderItem, vector< array buffer data >>>
	std::map<uint64_t, std::pair<size_t, std::vector<InstanceDrawInfo>>> instancingList;
	{
		ZoneScopedNC("Prepare", tracy::Color::AliceBlue);

		glActiveTexture(GL_TEXTURE0);
		this->FrameBuffer.BindTexture();

		ShaderManager* shdManager = ShaderManager::Get();

		{
			ZoneScopedN("SetUpInitialUniforms");

			for (uint32_t shaderId : Scene.UsedShaders)
			{
				ShaderProgram& shader = shdManager->GetShaderResource(shaderId);

				shader.SetUniform("RenderMatrix", RenderMatrix);
				shader.SetUniform("CameraPosition", glm::vec3(CameraTransform[3]));
				shader.SetUniform("Time", RunningTime);
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

					shader.SetUniform(
						LightPosLocs[lightIndex],
						lightData.Position
					);

					shader.SetUniform(
						LightColLocs[lightIndex],
						lightData.LightColor
					);

					shader.SetUniform(LightTypeLocs[lightIndex], (int)lightData.Type);

					shader.SetUniform(LightRangeLocs[lightIndex], lightData.Range);
					shader.SetUniform(LightAngLocs[lightIndex], lightData.Angle);
					shader.SetUniform(LightShadowsLocs[lightIndex], lightData.Shadows);
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
		}

		ZoneNamedN(bubzone, "BuildInstancingBuffer", true);

		for (size_t renderItemIndex = 0; renderItemIndex < Scene.RenderList.size(); renderItemIndex++)
		{
			const RenderItem& renderData = Scene.RenderList[renderItemIndex];

			// the MESH, MATERIAL, TRANSPARENCY and REFLECTIVITY must be the same
				// for a set of objects to be instanced together
				// And it needs to be on the GPU
			Mesh& mesh = meshProvider->GetMeshResource(renderData.RenderMeshId);

			uint64_t hash = 0;

			// make sure meshes that aren't on the gpu don't get
			// instanced
			// 21/01/2025 skinned meshes also can't rn
			if (mesh.GpuId == UINT32_MAX || !mesh.Bones.empty())
			{
				hash = instancingList.size();

				while (instancingList.find(hash) != instancingList.end())
					hash += 1;

				if (mesh.GpuId != UINT32_MAX && !mesh.Bones.empty())
				{
					const MeshProvider::GpuMesh& gpuMesh = meshProvider->GetGpuMesh(renderData.RenderMeshId);
					// 21/01/2025 dynamic bone transforms
					gpuMesh.VertexBuffer.SetBufferData(mesh.Vertices);
				}
			}
			else
				// yum
				hash = renderData.RenderMeshId
						+ static_cast<uint64_t>(renderData.MaterialId * 500u)
						+ (renderData.Transparency > 0.f ? 5000000ull : 0ull)
						+ static_cast<uint64_t>(renderData.MetallnessFactor * 115)
						+ static_cast<uint64_t>(renderData.RoughnessFactor * 115);

			auto it = instancingList.find(hash);
			if (it == instancingList.end())
			{
				instancingList[hash] = std::pair(renderItemIndex, std::vector<InstanceDrawInfo>());
				instancingList[hash].second.reserve(8);
			}

			std::vector<InstanceDrawInfo>& drawInfos = instancingList[hash].second;

			// Set buffer data
			drawInfos.emplace_back(
				renderData.Transform[0],
				renderData.Transform[1],
				renderData.Transform[2],
				renderData.Transform[3],
				renderData.Size,
				renderData.TintColor,
				renderData.Transparency
			);
		}
	}

	// 13/01/2025 `tracy::Color::Indigo`?? Indigo?? Park??
	ZoneNamedNC(perfzone, "Perform", tracy::Color::Indigo, true);

	for (auto& iter : instancingList)
	{
		ZoneNamedN(drawzone, "Draw", true);

		const RenderItem& renderData = Scene.RenderList[iter.second.first];
		const Mesh& mesh = meshProvider->GetMeshResource(renderData.RenderMeshId);

		const std::vector<InstanceDrawInfo>& drawInfos = iter.second.second;
		MeshProvider::GpuMesh& gpuMesh = meshProvider->GetGpuMesh(mesh.GpuId);

		{
			ZoneNamedNC(uploadzone, "UploadInstancingData", tracy::Color::Khaki, true);

			gpuMesh.VertexArray.Bind();

			glBindBuffer(GL_ARRAY_BUFFER, InstancingBuffer);

			glBufferData(
				GL_ARRAY_BUFFER,
				drawInfos.size() * sizeof(InstanceDrawInfo),
				drawInfos.data(),
				GL_STREAM_DRAW
			);
		}

		if (mesh.Bones.size() > 0)
		{
			ZoneNamedNC(uploadZoneSkinned, "UploadSkinningTransforms", tracy::Color::Khaki2, true);

			mtlManager->GetMaterialResource(renderData.MaterialId).ShaderId = ShaderManager::Get()->LoadFromPath("worldUberSkinned");

			glBindBuffer(GL_ARRAY_BUFFER, gpuMesh.SkinningBuffer);

			glBufferData(
				GL_ARRAY_BUFFER,
				gpuMesh.SkinningData.size() * sizeof(glm::mat4),
				gpuMesh.SkinningData.data(),
				GL_STREAM_DRAW
			);
		}

		RenderMaterial& material = mtlManager->GetMaterialResource(renderData.MaterialId);

		m_SetMaterialData(renderData, DebugWireframeRendering);

		this->DrawMesh(
			mesh,
			material.GetShader(),
			renderData.Size,
			renderData.Transform,
			renderData.FaceCulling,
			static_cast<int32_t>(drawInfos.size())
		);
	}
}

void Renderer::DrawMesh(
	const Mesh& Object,
	ShaderProgram& Shader,
	const glm::vec3& Size,
	const glm::mat4& Transform,
	FaceCullingMode FaceCulling,
	int32_t NumInstances
)
{
	ZoneScopedC(tracy::Color::HotPink);

	AccumulatedDrawCallCount++;

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

	[[unlikely]] default: {}

	}

	uint32_t gpuMeshId = Object.GpuId;
	MeshProvider::GpuMesh* gpuMesh = nullptr;

	// mesh not uploaded to the GPU by MeshProvider
	if (gpuMeshId == UINT32_MAX)
	{
		assert(false); // genuinely what the fuck is wrong with you

		m_VertexArray.Bind();

		m_VertexBuffer.SetBufferData(Object.Vertices);
		m_ElementBuffer.SetBufferData(Object.Indices);

		m_VertexBuffer.Bind();
		m_ElementBuffer.Bind();
	}
	else
	{
		gpuMesh = &MeshProvider::Get()->GetGpuMesh(gpuMeshId);
		gpuMesh->VertexArray.Bind();
		gpuMesh->VertexBuffer.Bind();
		gpuMesh->ElementBuffer.Bind();
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
		Shader.SetUniform("Scale", Size);
		Shader.Activate();

		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
	}
}

void Renderer::m_SetMaterialData(const RenderItem& RenderData, bool DebugWireframeRendering)
{
	ZoneScopedC(tracy::Color::HotPink);

	MaterialManager* mtlManager = MaterialManager::Get();

	RenderMaterial& material = mtlManager->GetMaterialResource(RenderData.MaterialId);
	ShaderProgram& shader = material.GetShader();

	if (!DebugWireframeRendering)
		switch (material.PolygonMode)
		{

		case RenderMaterial::MaterialPolygonMode::Fill:
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			break;
		}

		case RenderMaterial::MaterialPolygonMode::Lines:
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			break;
		}

		case RenderMaterial::MaterialPolygonMode::Points:
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
			break;
		}

		[[unlikely]] default: {}

		}
	else
		if (material.PolygonMode == RenderMaterial::MaterialPolygonMode::Points)
			glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
		else
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (RenderData.Transparency > 0.f || material.HasTranslucency)
		glEnable(GL_BLEND);
	else // the gosh darn grass model is practically 50% transparent
		glDisable(GL_BLEND);
	
	shader.SetUniform("SpecularMultiplier", material.SpecMultiply);
	shader.SetUniform("SpecularPower", material.SpecExponent);

	shader.SetUniform("MetallnessFactor", RenderData.MetallnessFactor);
	shader.SetUniform("RoughnessFactor", RenderData.RoughnessFactor);

	shader.SetUniform(
		"ColorTint",
		RenderData.TintColor
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

	//shader.SetTextureUniform("ColorMap", material.ColorMap);
	//shader.SetTextureUniform("MetallicRoughnessMap", material.MetallicRoughnessMap);

	TextureManager* texManager = TextureManager::Get();

	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, texManager->GetTextureResource(material.ColorMap).GpuId);

	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D, texManager->GetTextureResource(material.MetallicRoughnessMap).GpuId);

	if (material.NormalMap != 0)
	{
		shader.SetUniform("HasNormalMap", true);
		glActiveTexture(GL_TEXTURE12);
		glBindTexture(GL_TEXTURE_2D, texManager->GetTextureResource(material.NormalMap).GpuId);
	}
	else
		shader.SetUniform("HasNormalMap", false);

	if (material.EmissionMap != 0)
	{
		shader.SetUniform("HasEmissionMap", true);
		glActiveTexture(GL_TEXTURE13);
		glBindTexture(GL_TEXTURE_2D, texManager->GetTextureResource(material.EmissionMap).GpuId);
	}
	else
		shader.SetUniform("HasEmissionMap", false);

	// apply the uniforms for the shader program...
	shader.ApplyDefaultUniforms();
	// ... then the material uniforms...
	material.ApplyUniforms();
	// ... so that the material can override uniforms in the SP
}

void Renderer::SwapBuffers()
{
	ZoneScopedC(tracy::Color::HotPink);

	PHX_ENSURE(SDL_GL_SwapWindow(m_Window));
}
