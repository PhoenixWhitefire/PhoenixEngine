#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/gl.h>

#include "component/ParticleEmitter.hpp"
#include "component/Transform.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/MeshProvider.hpp"
#include "render/Renderer.hpp"

static std::default_random_engine s_RandGenerator = std::default_random_engine(static_cast<uint32_t>(time(NULL)));

class ParticleEmitterManager : public ComponentManager<EcParticleEmitter>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
        m_Components.back().Object = Object;

        return static_cast<uint32_t>(m_Components.size() - 1);
    }
	
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
            REFLECTION_PROPERTY_SIMPLE(EcParticleEmitter, Emitting, Boolean),
			REFLECTION_PROPERTY_SIMPLE(EcParticleEmitter, ParticlesAreAttached, Boolean),

			REFLECTION_PROPERTY(
				"Rate",
				Integer,
				[](void* g)
				-> Reflection::GenericValue
				{
					return static_cast<EcParticleEmitter*>(g)->Rate;
				},
				[](void* g, const Reflection::GenericValue& gv)
				{
					int64_t newRate = gv.AsInteger();
					if (newRate < 0 || newRate > UINT32_MAX)
						RAISE_RTF("Rate must be within uint32_t bounds (0 <= Rate <= 0xFFFFFFFFu), got {}", newRate);
					static_cast<EcParticleEmitter*>(g)->Rate = static_cast<uint32_t>(newRate);

				}
			),
		
			REFLECTION_PROPERTY(
				"Image",
				String,
				[](void* p) -> Reflection::GenericValue
				{
					EcParticleEmitter* ep = static_cast<EcParticleEmitter*>(p);
					const Texture& tex = TextureManager::Get()->GetTextureResource(ep->Image);

					return tex.ImagePath;
				},
				[](void* p, const Reflection::GenericValue& gv)
				{
					EcParticleEmitter* ep = static_cast<EcParticleEmitter*>(p);
					ep->Image = TextureManager::Get()->LoadFromPath(gv.AsString());
				}
			),

			REFLECTION_PROPERTY(
				"BilinearFiltering",
				Boolean,
				[](void* p) -> Reflection::GenericValue
				{
					EcParticleEmitter* ep = static_cast<EcParticleEmitter*>(p);
					const Texture& tex = TextureManager::Get()->GetTextureResource(ep->Image);

					return tex.DoBilinearSmoothing;
				},
				[](void* p, const Reflection::GenericValue& gv)
				{
					TextureManager* textureManager = TextureManager::Get();

					EcParticleEmitter* ep = static_cast<EcParticleEmitter*>(p);
					const Texture& tex = textureManager->GetTextureResource(ep->Image);

					ep->Image = textureManager->LoadFromPath(tex.ImagePath, true, gv.AsBoolean());
				}
			),

			REFLECTION_PROPERTY_SIMPLE(EcParticleEmitter, Lifetime, Vector2)
        };

        return props;
    }
};

static inline ParticleEmitterManager Instance;

EcParticleEmitter::EcParticleEmitter()
{
	this->VelocityOverTime.InsertKey(0.00f, glm::vec3(0.f, 5.0f, 0.f), glm::vec3(15.f));
	this->VelocityOverTime.InsertKey(0.75f, glm::vec3(0.f, 2.5f, 0.f));
	this->VelocityOverTime.InsertKey(1.00f, glm::vec3(0.f, 0.0f, 0.f));

	this->TransparencyOverTime.InsertKey(0.0f, 0.0f);
	this->TransparencyOverTime.InsertKey(0.8f, 0.5f);
	this->TransparencyOverTime.InsertKey(1.0f, 1.0f);

	this->SizeOverTime.InsertKey(0.0f, 1.0f);
	this->SizeOverTime.InsertKey(1.0f, 15.f);
}

size_t EcParticleEmitter::m_GetUsableParticleIndex()
{
	if (m_Particles.empty()) // Are there any particles at all?
	{
		m_Particles.resize(1); // Resize array to length 1

		return 0;
	}
	else
	{
		for (size_t index = 0; index < m_Particles.size(); index++)
		{
			Particle& particle = m_Particles[index];

			if (particle.TimeAliveFor > particle.Lifetime)
			{ // Get first inactive particle

				//m_Particles.erase(index + m_Particles.begin());

				return index;
			}
		}
	}

	m_Particles.resize(m_Particles.size() + 1);

	return m_Particles.size() - 1;
}

void EcParticleEmitter::Update(double DeltaTime)
{
	float deltaTimeForGLM = static_cast<float>(DeltaTime);
	float timeBetweenSpawn = 1.f / this->Rate;

	if (m_TimeSinceLastSpawn >= timeBetweenSpawn && this->Emitting)
	{
		uint32_t numToSpawn = static_cast<uint32_t>(m_TimeSinceLastSpawn / timeBetweenSpawn);

		m_TimeSinceLastSpawn = 0.f;

		// Spawn a new particle
		std::uniform_real_distribution<float> lifetimeDist(this->Lifetime.x, this->Lifetime.y);

		EcTransform* ct = Object->FindComponent<EcTransform>();

		for (uint32_t i = 0; i < numToSpawn; i++)
		{
			Particle newParticle = {
				.Lifetime = lifetimeDist(s_RandGenerator),
				.TimeAliveFor = 0.f,
				.Size = 1.f,
				.Transparency = 0.f,
				.Tint = Color(1.f, 1.f, 1.f),
				.Position = this->ParticlesAreAttached ? glm::vec3() : glm::vec3(ct->Transform[3])
			};

			m_Particles[m_GetUsableParticleIndex()] = newParticle;
		}
	}
	else
		m_TimeSinceLastSpawn += DeltaTime;

	for (Particle& particle : m_Particles)
	{
		particle.TimeAliveFor += static_cast<float>(DeltaTime);

		float lifeProgress = particle.TimeAliveFor / particle.Lifetime;

		particle.Position = particle.Position + (this->VelocityOverTime.GetValue(lifeProgress) * deltaTimeForGLM);
		particle.Size = this->SizeOverTime.GetValue(lifeProgress);

		particle.Transparency = this->TransparencyOverTime.GetValue(lifeProgress);
		particle.Tint = this->ColorOverTime.GetValue(lifeProgress);
	}
}

void EcParticleEmitter::Render(const glm::mat4& RenderMatrix)
{
	if (m_Particles.empty())
		return;

	EcTransform* ct = Object->FindComponent<EcTransform>();
	if (!ct)
		return;

	static MeshProvider* meshProvider = MeshProvider::Get();
	static ShaderManager* shdManager = ShaderManager::Get();
	static Renderer* renderer = Renderer::Get();

	static uint32_t QuadMeshId = meshProvider->LoadFromPath("!Quad");
	static uint32_t ShaderId = shdManager->LoadFromPath("particle");

	ShaderProgram& particleShader = shdManager->GetShaderResource(ShaderId);
	const Mesh& quadMesh = meshProvider->GetMeshResource(QuadMeshId);
	const MeshProvider::GpuMesh& quadGpu = meshProvider->GetGpuMesh(quadMesh.GpuId);

	quadGpu.VertexArray.Bind();
	quadGpu.VertexBuffer.Bind();
	quadGpu.ElementBuffer.Bind();
	particleShader.SetUniform("RenderMatrix", RenderMatrix);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);

	for (size_t index = 0; index < m_Particles.size(); index++)
	{
		Particle& particle = m_Particles[index];

		if (particle.TimeAliveFor > particle.Lifetime)
		{
			//m_Particles.erase(m_Particles.begin() + index);

			//delete particle;

			continue;
		}

		/*
		particleShaders.SetTextureUniform(
			"Image",
			TextureManager::Get()->GetTextureResource(particle.Image).GpuId,
			Texture::DimensionType::Texture2D
		);
		*/

		glm::mat4 transform = glm::translate(glm::mat4(1.f), particle.Position);

		if (ParticlesAreAttached)
			transform *= ct->Transform;

		particleShader.SetUniform("Position", glm::vec3(transform[3]));
		particleShader.SetUniform("Size", particle.Size);
		particleShader.SetUniform("Tint", particle.Tint.ToGenericValue());
		particleShader.SetUniform("Transparency", particle.Transparency);
		particleShader.SetTextureUniform("Image", Image);
		particleShader.Activate();

		glDrawElements(GL_TRIANGLES, quadGpu.NumIndices, GL_UNSIGNED_INT, 0);
		renderer->AccumulatedDrawCallCount++;
	}
}
