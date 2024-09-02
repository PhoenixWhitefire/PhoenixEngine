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
			if (newRate < 0 || newRate > 0xFFFFFFu)
				throw("ParticleEmitter.Rate must be within uint32_t bounds (0 < Rate <= 0xFFFFFFu)");
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
			if (newMax < 0 || newMax > 0xFFFFFFu)
				throw("ParticleEmitter.MaxParticles must be within uint32_t bounds (0 < MaxParticles <= 0xFFFFFFu)");
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

	this->VertArray.Bind();

	this->VertArray.LinkAttrib(this->VertBuffer, 0, 2, GL_FLOAT, 2 * sizeof(float), (void*)0);

	this->VertBuffer.Bind();
	glBufferData(GL_ARRAY_BUFFER, sizeof(Quad), &Quad, GL_STREAM_DRAW);

	GLuint quadinds[] =
	{
		0, 1, 2,
		2, 3, 0
	};

	this->ElementBuffer.Bind();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadinds), &quadinds, GL_STREAM_DRAW);

	Texture* DefaultImage = TextureManager::Get()->LoadTextureFromPath(MissingTexPath);

	this->PossibleImages = { DefaultImage };

	this->VertArray.Unbind();
	this->VertBuffer.Unbind();
	this->ElementBuffer.Unbind();

	this->VelocityOverTime.InsertKey(ValueSequenceKeypoint<Vector3>(0, Vector3(0, 5, 0)));
	this->VelocityOverTime.InsertKey(ValueSequenceKeypoint<Vector3>(0.75, Vector3(0, 2.5, 0)));
	this->VelocityOverTime.InsertKey(ValueSequenceKeypoint<Vector3>(1, Vector3(0, 0, 0)));

	this->TransparencyOverTime.InsertKey(ValueSequenceKeypoint<float>(0.f, 0.f));
	this->TransparencyOverTime.InsertKey(ValueSequenceKeypoint<float>(.8f, .5f));
	this->TransparencyOverTime.InsertKey(ValueSequenceKeypoint<float>(1.f, 1.f));

	s_DeclareReflections();
}

size_t Object_ParticleEmitter::m_getUsableIndex()
{
	if (this->m_particles.size() == 0) //Are there any particles at all?
	{
		this->m_particles.resize(1); //Resize array to length 1

		return 0;
	}
	else
	{
		if ((!this->m_particles[0]) || this->m_particles[0]->TimeAliveFor > this->m_particles[0]->Lifetime)
		{ //Check if oldest particle shouldn't be active (ideal)
			delete this->m_particles[0];

			this->m_particles[0] = nullptr;

			return 0;
		}
		else
		{
			for (int Index = 0; Index < this->m_particles.size(); Index++)
			{
				if (this->m_particles[Index] == nullptr)
					continue;

				if (this->m_particles[Index]->TimeAliveFor > this->m_particles[Index]->Lifetime)
				{ //Get first inactive particle
					delete this->m_particles[Index];

					this->m_particles[Index] = nullptr;

					return Index;
				}
			}
		}
	}

	if (this->m_particles.size() < this->MaxParticles)
	{
		this->m_particles.resize(this->m_particles.size() + 1);

		return this->m_particles.size() - 1;
	}

	return 0;
}

void Object_ParticleEmitter::Update(double Delta)
{
	float TimeBetweenSpawn = 1.0f / this->Rate;

	Object_Base3D* Parent3D = dynamic_cast<Object_Base3D*>(this->GetParent());

	Vector3 WorldPosition = Parent3D ? Vector3(glm::vec3(Parent3D->Transform[3])) + this->Offset : this->Offset;

	_particle* NewParticle = nullptr;

	if (this->m_lastSpawnedDelta >= TimeBetweenSpawn && this->PossibleImages.size() > 0 && EmitterEnabled)
	{
		this->m_lastSpawnedDelta = 0.0f;

		//Spawn a new particle

		float numimages = (float)this->PossibleImages.size();

		std::uniform_real_distribution<double> rand_idx(0, numimages);
		std::uniform_real_distribution<float> lifetimeDist(this->Lifetime.X, this->Lifetime.Y);

		NewParticle = new _particle();
		NewParticle->Image = this->PossibleImages[(uint32_t)rand_idx(Object_ParticleEmitter::s_RandGenerator)];
		NewParticle->Position = WorldPosition;
		NewParticle->Lifetime = lifetimeDist(Object_ParticleEmitter::s_RandGenerator);

		this->m_particles[this->m_getUsableIndex()] = NewParticle;
	}
	else
	{
		this->m_lastSpawnedDelta += Delta;
	}

	for (int Index = 0; Index < this->m_particles.size(); Index++)
	{
		_particle* Particle = this->m_particles[Index];

		if (Particle != nullptr && Particle != NewParticle)
		{
			Particle->TimeAliveFor += static_cast<float>(Delta);

			float LifeProgress = Particle->TimeAliveFor / Particle->Lifetime;

			Particle->Position = Particle->Position + (this->VelocityOverTime.GetValue(LifeProgress) * Delta);
			Particle->Size = this->SizeOverTime.GetValue(LifeProgress);

			Particle->Transparency = this->TransparencyOverTime.GetValue(LifeProgress);
		}
	}
}

void Object_ParticleEmitter::Render(glm::mat4 CameraMatrix)
{
	if (this->m_particles.size() == 0)
		return;

	this->VertArray.Bind();
	this->VertBuffer.Bind();
	this->ElementBuffer.Bind();

	Object_ParticleEmitter::s_ParticleShaders->Activate();

	glEnable(GL_BLEND);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	for (int Index = 0; Index < this->m_particles.size(); Index++)
	{
		_particle* Particle = this->m_particles[Index];

		if (Particle == nullptr || Particle->TimeAliveFor > Particle->Lifetime)
		{
			delete this->m_particles[Index];
			this->m_particles[Index] = nullptr;

			continue;
		}

		Object_ParticleEmitter::s_ParticleShaders->Activate();

		glUniform3f(
			glGetUniformLocation(Object_ParticleEmitter::s_ParticleShaders->ID, "Position"),
			static_cast<float>(Particle->Position.X),
			static_cast<float>(Particle->Position.Y),
			static_cast<float>(Particle->Position.Z)
		);

		glUniform1f(
			glGetUniformLocation(Object_ParticleEmitter::s_ParticleShaders->ID, "Transparency"),
			Particle->Transparency
		);

		glm::mat4 Scale = glm::mat4(1.0f);

		Scale = glm::scale(Scale, glm::vec3(Particle->Size, Particle->Size, Particle->Size));

		glUniformMatrix4fv(
			glGetUniformLocation(Object_ParticleEmitter::s_ParticleShaders->ID, "Scale"),
			1,
			GL_FALSE,
			glm::value_ptr(Scale)
		);

		glUniformMatrix4fv(
			glGetUniformLocation(Object_ParticleEmitter::s_ParticleShaders->ID, "CameraMatrix"),
			1,
			GL_FALSE,
			glm::value_ptr(CameraMatrix)
		);

		glm::vec4 CameraPos = CameraMatrix[3];

		glUniform3f(
			glGetUniformLocation(Object_ParticleEmitter::s_ParticleShaders->ID, "CameraPosition"),
			CameraPos.x,
			CameraPos.y,
			CameraPos.z
		);

		glBindTexture(GL_TEXTURE_2D, Particle->Image->Identifier);

		GLint ActiveID;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &ActiveID);

		glUniform1i(glGetUniformLocation(Object_ParticleEmitter::s_ParticleShaders->ID, "Image"), ActiveID);

		glDrawElements(GL_TRIANGLES, 7, GL_UNSIGNED_INT, 0);
	}

	this->VertArray.Unbind();
	this->VertBuffer.Unbind();
	this->ElementBuffer.Unbind();

	glDisable(GL_BLEND);
}
