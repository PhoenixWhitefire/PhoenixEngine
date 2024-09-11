#include<glad/gl.h>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtc/matrix_transform.hpp>

#include"gameobject/ParticleEmitter.hpp"
#include"gameobject/Base3D.hpp"
#include"datatype/Vector2.hpp"
#include"datatype/Buffer.hpp"

RegisterDerivedObject<Object_ParticleEmitter> Object_ParticleEmitter::RegisterClassAs("ParticleEmitter");

ShaderProgram* Object_ParticleEmitter::s_ParticleShaders = nullptr;
std::default_random_engine Object_ParticleEmitter::s_RandGenerator = std::default_random_engine(static_cast<uint32_t>(time(NULL)));
static bool s_DidInitReflection = false;

static const std::string MissingTexPath = "textures/MISSING2_MaximumADHD_status_1665776378145304579.png";

float Quad[] =
{
	//Triangle 1 - left
	-0.5f,  0.5f,
	-0.5f, -0.5f,
	 0.5f, -0.5f,
	 0.5f,  0.5f,
};

template <class T> static void removeElement(std::vector<T> vec, size_t elem)
{
	_particle prevLast = vec[vec.size() - 1];

	vec[vec.size() - 1] = vec[elem];
	vec[elem] = prevLast;

	vec.pop_back();
}

void Object_ParticleEmitter::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROP_SIMPLE(Object_ParticleEmitter, EmitterEnabled, Bool);

	REFLECTION_DECLAREPROP(
		"Rate",
		Integer,
		[](GameObject* g)
		{
			return dynamic_cast<Object_ParticleEmitter*>(g)->Rate;
		},
		[](GameObject* g, Reflection::GenericValue gv)
		{
			int64_t newRate = gv.Integer;
			if (newRate < 0 || newRate > UINT32_MAX)
				throw("ParticleEmitter.Rate must be within uint32_t bounds (0 <= Rate <= 0xFFFFFFFFu)");
			dynamic_cast<Object_ParticleEmitter*>(g)->Rate = static_cast<uint32_t>(newRate);
				
		}
	);

	REFLECTION_DECLAREPROP(
		"MaxParticles",
		Integer,
		[](GameObject* g)
		{
			return dynamic_cast<Object_ParticleEmitter*>(g)->MaxParticles;
		},
		[](GameObject* g, Reflection::GenericValue gv)
		{
			int64_t newMax = gv.Integer;
			if (newMax < 0 || newMax > UINT32_MAX)
				throw("ParticleEmitter.MaxParticles must be within uint32_t bounds (0 < MaxParticles <= 0xFFFFFFFFu)");
			dynamic_cast<Object_ParticleEmitter*>(g)->MaxParticles = static_cast<uint32_t>(newMax);
		}
	);

	REFLECTION_DECLAREPROP(
		"Lifetime",
		Vector3,
		[](GameObject* g)
		{
			Object_ParticleEmitter* p = dynamic_cast<Object_ParticleEmitter*>(g);
			return Vector3(p->Lifetime.X, p->Lifetime.Y, 0.f).ToGenericValue();
		},
		[](GameObject* g, Reflection::GenericValue gv)
		{
			Vector3 newLifetime = gv;
			dynamic_cast<Object_ParticleEmitter*>(g)->Lifetime = Vector2(
				static_cast<float>(newLifetime.X),
				static_cast<float>(newLifetime.Y)
			);
		}
	);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_ParticleEmitter, Offset, Vector3);
}

Object_ParticleEmitter::Object_ParticleEmitter()
{
	this->Name = "ParticleEmitter";
	this->ClassName = "ParticleEmitter";

	if (!Object_ParticleEmitter::s_ParticleShaders)
		Object_ParticleEmitter::s_ParticleShaders = ShaderProgram::GetShaderProgram("particle");

	m_VertArray.Bind();

	m_VertArray.LinkAttrib(m_VertBuffer, 0, 2, GL_FLOAT, 2 * sizeof(float), (void*)0);

	m_VertBuffer.Bind();
	glBufferData(GL_ARRAY_BUFFER, sizeof(Quad), &Quad, GL_STREAM_DRAW);

	GLuint quadinds[] =
	{
		0, 1, 2,
		2, 3, 0
	};

	m_ElementBuffer.Bind();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadinds), &quadinds, GL_STREAM_DRAW);

	Texture* DefaultImage = TextureManager::Get()->LoadTextureFromPath(MissingTexPath);

	this->PossibleImages = { DefaultImage };

	m_VertArray.Unbind();
	m_VertBuffer.Unbind();
	m_ElementBuffer.Unbind();

	this->VelocityOverTime.InsertKey(ValueSequenceKeypoint<Vector3>(0, Vector3(0, 5, 0)));
	this->VelocityOverTime.InsertKey(ValueSequenceKeypoint<Vector3>(0.75, Vector3(0, 2.5, 0)));
	this->VelocityOverTime.InsertKey(ValueSequenceKeypoint<Vector3>(1, Vector3(0, 0, 0)));

	this->TransparencyOverTime.InsertKey(ValueSequenceKeypoint<float>(0.f, 0.f));
	this->TransparencyOverTime.InsertKey(ValueSequenceKeypoint<float>(.8f, .5f));
	this->TransparencyOverTime.InsertKey(ValueSequenceKeypoint<float>(1.f, 1.f));

	this->SizeOverTime.InsertKey(ValueSequenceKeypoint<float>(0.f, 1.f));
	this->SizeOverTime.InsertKey(ValueSequenceKeypoint<float>(1.f, 1.f));

	s_DeclareReflections();
}

size_t Object_ParticleEmitter::m_GetUsableParticleIndex()
{
	if (m_Particles.size() == 0) //Are there any particles at all?
	{
		m_Particles.resize(1); //Resize array to length 1

		return 0;
	}
	else
	{
		for (size_t index = 0; index < m_Particles.size(); index++)
		{
			_particle& particle = m_Particles[index];

			if (particle.TimeAliveFor > particle.Lifetime)
			{ //Get first inactive particle

				removeElement(m_Particles, index);

				return index;
			}
		}
	}

	if (m_Particles.size() < this->MaxParticles)
	{
		m_Particles.resize(m_Particles.size() + 1);

		return m_Particles.size() - 1;
	}

	return 0;
}

void Object_ParticleEmitter::Update(double DeltaTime)
{
	float timeBetweenSpawn = 1.0f / this->Rate;

	Object_Base3D* parent3D = dynamic_cast<Object_Base3D*>(this->GetParent());

	Vector3 worldPosition = parent3D ? Vector3(glm::vec3(parent3D->Transform[3])) + this->Offset : this->Offset;

	size_t newParticleIndex = UINT32_MAX;

	if (m_TimeSinceLastSpawn >= timeBetweenSpawn && this->PossibleImages.size() > 0 && this->EmitterEnabled)
	{
		m_TimeSinceLastSpawn = 0.0f;

		//Spawn a new particle

		float numimages = (float)this->PossibleImages.size();

		std::uniform_real_distribution<double> randIndex(0, numimages);
		std::uniform_real_distribution<float> lifetimeDist(this->Lifetime.X, this->Lifetime.Y);

		_particle newParticle
		{
			lifetimeDist(s_RandGenerator),
			0.f,
			1.f,
			0.f,
			this->PossibleImages[(uint32_t)randIndex(s_RandGenerator)]
		};

		newParticleIndex = m_GetUsableParticleIndex();
		m_Particles[newParticleIndex] = newParticle;
	}
	else
		m_TimeSinceLastSpawn += DeltaTime;

	for (_particle& particle : m_Particles)
	{
		particle.TimeAliveFor += static_cast<float>(DeltaTime);

		float LifeProgress = particle.TimeAliveFor / particle.Lifetime;

		particle.Position = particle.Position + (this->VelocityOverTime.GetValue(LifeProgress) * DeltaTime);
		particle.Size = this->SizeOverTime.GetValue(LifeProgress);

		particle.Transparency = this->TransparencyOverTime.GetValue(LifeProgress);
	}
}

void Object_ParticleEmitter::Render(glm::mat4 CameraMatrix)
{
	if (m_Particles.size() == 0)
		return;

	m_VertArray.Bind();
	m_VertBuffer.Bind();
	m_ElementBuffer.Bind();

	s_ParticleShaders->Activate();

	glEnable(GL_BLEND);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	for (size_t index = 0; index < m_Particles.size(); index++)
	{
		_particle& particle = m_Particles[index];

		if (particle.TimeAliveFor > particle.Lifetime)
		{
			removeElement(m_Particles, index);

			//delete particle;

			continue;
		}

		s_ParticleShaders->Activate();

		s_ParticleShaders->SetUniformFloat3(
			"Position",
			static_cast<float>(particle.Position.X),
			static_cast<float>(particle.Position.Y),
			static_cast<float>(particle.Position.Z)
		);

		s_ParticleShaders->SetUniformFloat("Transparency", particle.Transparency);

		glm::mat4 scale = glm::scale(glm::mat4(1.f), glm::vec3(particle.Size, particle.Size, particle.Size));

		s_ParticleShaders->SetUniformMatrix("Scale", scale);

		s_ParticleShaders->SetUniformMatrix("CameraMatrix", CameraMatrix);

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, particle.Image->Identifier);

		s_ParticleShaders->SetUniformInt("Image", 4);

		glDrawElements(GL_TRIANGLES, 7, GL_UNSIGNED_INT, 0);
	}

	m_VertArray.Unbind();
	m_VertBuffer.Unbind();
	m_ElementBuffer.Unbind();

	glDisable(GL_BLEND);
}
