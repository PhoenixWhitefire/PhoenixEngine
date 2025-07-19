#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "component/ParticleEmitter.hpp"
#include "component/Transform.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/MeshProvider.hpp"

static uint32_t s_ParticleShaders = 0;
static std::default_random_engine s_RandGenerator = std::default_random_engine(static_cast<uint32_t>(time(NULL)));

static uint32_t QuadMeshId = 0;

class ParticleEmitterManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
        m_Components.back().Object = Object;

		if (!Object->GetComponent<EcTransform>())
			Object->AddComponent(EntityComponent::Transform);

        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcParticleEmitter& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

    virtual void* GetComponent(uint32_t Id) override
    {
        return &m_Components[Id];
    }

    virtual void DeleteComponent(uint32_t Id) override
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth

		m_Components[Id].Object.Invalidate();
    }

	virtual void Shutdown() override
    {
        for (size_t i = 0; i < m_Components.size(); i++)
            DeleteComponent(i);
    }

    virtual const Reflection::PropertyMap& GetProperties() override
    {
        static const Reflection::PropertyMap props = 
        {
            EC_PROP_SIMPLE(EcParticleEmitter, Emitting, Boolean),
			EC_PROP_SIMPLE(EcParticleEmitter, ParticlesAreAttached, Boolean),

			EC_PROP(
				"Rate",
				Integer,
				[](void* g)
				{
					return static_cast<EcParticleEmitter*>(g)->Rate;
				},
				[](void* g, const Reflection::GenericValue& gv)
				{
					int64_t newRate = gv.AsInteger();
					if (newRate < 0 || newRate > UINT32_MAX)
						RAISE_RT("ParticleEmitter.Rate must be within uint32_t bounds (0 <= Rate <= 0xFFFFFFFFu)");
					static_cast<EcParticleEmitter*>(g)->Rate = static_cast<uint32_t>(newRate);

				}
			),
		
			/*
			REFLECTION_DECLAREPROP(
				"MaxParticles",
				Integer,
				[](Reflection::Reflectable* g)
				{
					return dynamic_cast<Object_ParticleEmitter*>(g)->MaxParticles;
				},
				[](Reflection::Reflectable* g, Reflection::GenericValue gv)
				{
					int64_t newMax = gv.AsInteger();
					if (newMax < 0 || newMax > UINT32_MAX)
						RAISE_RT("ParticleEmitter.MaxParticles must be within uint32_t bounds (0 < MaxParticles <= 0xFFFFFFFFu)");
					dynamic_cast<Object_ParticleEmitter*>(g)->MaxParticles = static_cast<uint32_t>(newMax);
				}
			);
			*/

			EC_PROP_SIMPLE(EcParticleEmitter, Lifetime, Vector2)
        };

        return props;
    }

    virtual const Reflection::MethodMap& GetMethods() override
    {
        static const Reflection::MethodMap funcs = {};
        return funcs;
    }

    ParticleEmitterManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::ParticleEmitter] = this;
    }

private:
    std::vector<EcParticleEmitter> m_Components;
};

static inline ParticleEmitterManager Instance{};

EcParticleEmitter::EcParticleEmitter()
{
	if (s_ParticleShaders == 0)
		s_ParticleShaders = ShaderManager::Get()->LoadFromPath("particle");

	this->PossibleImages = { 1 };

	this->VelocityOverTime.InsertKey(0.00f, glm::vec3(0.f, 5.0f, 0.f), 15.f);
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
	if (m_Particles.empty()) //Are there any particles at all?
	{
		m_Particles.resize(1); //Resize array to length 1

		return 0;
	}
	else
	{
		for (size_t index = 0; index < m_Particles.size(); index++)
		{
			Particle& particle = m_Particles[index];

			if (particle.TimeAliveFor > particle.Lifetime)
			{ //Get first inactive particle

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

	if (m_TimeSinceLastSpawn >= timeBetweenSpawn && !this->PossibleImages.empty() && this->Emitting)
	{
		uint32_t numToSpawn = static_cast<uint32_t>(m_TimeSinceLastSpawn / timeBetweenSpawn);

		m_TimeSinceLastSpawn = 0.f;

		//Spawn a new particle

		float numimages = (float)this->PossibleImages.size();

		std::uniform_real_distribution<float> randIndex(0.f, numimages);
		std::uniform_real_distribution<float> lifetimeDist(this->Lifetime.x, this->Lifetime.y);

		EcTransform* ct = Object->GetComponent<EcTransform>();

		for (uint32_t i = 0; i < numToSpawn; i++)
		{
			Particle newParticle
			{
				lifetimeDist(s_RandGenerator),
				0.f,
				1.f,
				0.f,
				this->PossibleImages[(uint32_t)randIndex(s_RandGenerator)],
				this->ParticlesAreAttached ? glm::vec3{} : glm::vec3(ct->Transform[3])
			};

			m_Particles[m_GetUsableParticleIndex()] = newParticle;
		}
	}
	else
		m_TimeSinceLastSpawn += DeltaTime;

	for (Particle& particle : m_Particles)
	{
		particle.TimeAliveFor += static_cast<float>(DeltaTime);

		float LifeProgress = particle.TimeAliveFor / particle.Lifetime;

		particle.Position = particle.Position + (this->VelocityOverTime.GetValue(LifeProgress) * deltaTimeForGLM);
		particle.Size = this->SizeOverTime.GetValue(LifeProgress);

		particle.Transparency = this->TransparencyOverTime.GetValue(LifeProgress);
	}
}

void EcParticleEmitter::AppendToRenderList(std::vector<RenderItem>& RenderList)
{
	if (m_Particles.empty())
		return;

	RenderList.reserve(RenderList.size() + m_Particles.size());

	if (QuadMeshId == 0)
	{
		MeshProvider* mp = MeshProvider::Get();
		QuadMeshId = mp->LoadFromPath("!Quad");
	}

	EcTransform* ct = Object->GetComponent<EcTransform>();

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

		RenderList.emplace_back(
			QuadMeshId,
			transform,
			glm::vec3{ particle.Size },
			MaterialManager::Get()->LoadFromPath("error"),
			Color(1.f, 1.f, 1.f),
			particle.Transparency,
			0.f,
			0.f,
			FaceCullingMode::None
		);
	}
}
