#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "gameobject/ParticleEmitter.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/MeshProvider.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(ParticleEmitter);

static uint32_t s_ParticleShaders = 0;
static std::default_random_engine s_RandGenerator = std::default_random_engine(static_cast<uint32_t>(time(NULL)));
static bool s_DidInitReflection = false;

static uint32_t QuadMeshId = 0;

void Object_ParticleEmitter::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(Attachment);

	REFLECTION_DECLAREPROP_SIMPLE(Object_ParticleEmitter, EmitterEnabled, Bool);
	REFLECTION_DECLAREPROP_SIMPLE(Object_ParticleEmitter, ParticlesRelativeToEmitter, Bool);

	REFLECTION_DECLAREPROP(
		"Rate",
		Integer,
		[](Reflection::Reflectable* g)
		{
			return static_cast<Object_ParticleEmitter*>(g)->Rate;
		},
		[](Reflection::Reflectable* g, Reflection::GenericValue gv)
		{
			int64_t newRate = gv.AsInteger();
			if (newRate < 0 || newRate > UINT32_MAX)
				throw("ParticleEmitter.Rate must be within uint32_t bounds (0 <= Rate <= 0xFFFFFFFFu)");
			static_cast<Object_ParticleEmitter*>(g)->Rate = static_cast<uint32_t>(newRate);
				
		}
	);

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
				throw("ParticleEmitter.MaxParticles must be within uint32_t bounds (0 < MaxParticles <= 0xFFFFFFFFu)");
			dynamic_cast<Object_ParticleEmitter*>(g)->MaxParticles = static_cast<uint32_t>(newMax);
		}
	);
	*/

	REFLECTION_DECLAREPROP(
		"Lifetime",
		Vector3,
		[](Reflection::Reflectable* g)
		{
			Object_ParticleEmitter* p = static_cast<Object_ParticleEmitter*>(g);
			return Vector3(p->Lifetime.X, p->Lifetime.Y, 0.f).ToGenericValue();
		},
		[](Reflection::Reflectable* g, Reflection::GenericValue gv)
		{
			Vector3 newLifetime = gv;
			static_cast<Object_ParticleEmitter*>(g)->Lifetime = Vector2(
				static_cast<float>(newLifetime.X),
				static_cast<float>(newLifetime.Y)
			);
		}
	);
}

Object_ParticleEmitter::Object_ParticleEmitter()
{
	this->Name = "ParticleEmitter";
	this->ClassName = "ParticleEmitter";

	if (s_ParticleShaders == 0)
		s_ParticleShaders = ShaderManager::Get()->LoadFromPath("particle");

	this->PossibleImages = { 1 };

	this->VelocityOverTime.InsertKey(ValueSequenceKeypoint<glm::vec3>(0.00f, glm::vec3(0.f, 5.0f, 0.f)));
	this->VelocityOverTime.InsertKey(ValueSequenceKeypoint<glm::vec3>(0.75f, glm::vec3(0.f, 2.5f, 0.f)));
	this->VelocityOverTime.InsertKey(ValueSequenceKeypoint<glm::vec3>(1.00f, glm::vec3(0.f, 0.0f, 0.f)));

	this->TransparencyOverTime.InsertKey(ValueSequenceKeypoint<float>(0.f, 0.f));
	this->TransparencyOverTime.InsertKey(ValueSequenceKeypoint<float>(.8f, .5f));
	this->TransparencyOverTime.InsertKey(ValueSequenceKeypoint<float>(1.f, 1.f));

	this->SizeOverTime.InsertKey(ValueSequenceKeypoint<float>(0.f, 1.f));
	this->SizeOverTime.InsertKey(ValueSequenceKeypoint<float>(1.f, 15.f));

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

size_t Object_ParticleEmitter::m_GetUsableParticleIndex()
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

void Object_ParticleEmitter::Update(double DeltaTime)
{
	float deltaTimeForGLM = static_cast<float>(DeltaTime);
	float timeBetweenSpawn = 1.f / this->Rate;

	size_t newParticleIndex = UINT32_MAX;

	if (m_TimeSinceLastSpawn >= timeBetweenSpawn && !this->PossibleImages.empty() && this->EmitterEnabled)
	{
		m_TimeSinceLastSpawn = 0.f;

		//Spawn a new particle

		float numimages = (float)this->PossibleImages.size();

		std::uniform_real_distribution<double> randIndex(0, numimages);
		std::uniform_real_distribution<float> lifetimeDist(this->Lifetime.X, this->Lifetime.Y);

		Particle newParticle
		{
			lifetimeDist(s_RandGenerator),
			0.f,
			1.f,
			0.f,
			this->PossibleImages[(uint32_t)randIndex(s_RandGenerator)],
			this->ParticlesRelativeToEmitter ? glm::vec3{} : glm::vec3(this->GetWorldTransform()[3])
		};

		newParticleIndex = m_GetUsableParticleIndex();
		m_Particles[newParticleIndex] = newParticle;
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

std::vector<RenderItem> Object_ParticleEmitter::GetRenderList()
{
	if (m_Particles.empty())
		return {};

	std::vector<RenderItem> rlist;
	rlist.reserve(m_Particles.size());

	if (QuadMeshId == 0)
	{
		MeshProvider* mp = MeshProvider::Get();
		QuadMeshId = mp->LoadFromPath("!Quad");
	}

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

		if (ParticlesRelativeToEmitter)
			transform *= this->GetWorldTransform();

		rlist.emplace_back(
			QuadMeshId,
			transform,
			Vector3::one * particle.Size,
			MaterialManager::Get()->LoadMaterialFromPath("error"),
			Color(1.f, 1.f, 1.f),
			particle.Transparency,
			0.f,
			0.f,
			FaceCullingMode::None
		);
	}

	return rlist;
}
