#include"gameobject/ParticleEmitter.hpp"
#include"gameobject/Base3D.hpp"

DerivedObjectRegister<Object_ParticleEmitter> Object_ParticleEmitter::RegisterClassAs("ParticleEmitter");

ShaderProgram* Object_ParticleEmitter::ParticleShaders = nullptr;

float Quad[] = {
	//Triangle 1 - left
	-0.5f,  0.5f,
	-0.5f, -0.5f,
	 0.5f, -0.5f,
	 0.5f,  0.5f,
};

std::default_random_engine Object_ParticleEmitter::rand_generator = std::default_random_engine(time(NULL));

Object_ParticleEmitter::Object_ParticleEmitter()
{
	if (!Object_ParticleEmitter::ParticleShaders)
		Object_ParticleEmitter::ParticleShaders = new ShaderProgram("particle.vert", "particle.frag");

	this->VertArray.Bind();

	this->VertArray.LinkAttrib(this->VertBuffer, 0, 2, GL_FLOAT, 2 * sizeof(float), (void*)0);

	this->VertBuffer.Bind();
	glBufferData(GL_ARRAY_BUFFER, sizeof(Quad), &Quad, GL_STREAM_DRAW);

	GLuint quadinds[] = {
		0, 1, 2,
		2, 3, 0
	};

	this->ElementBuffer.Bind();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadinds), &quadinds, GL_STREAM_DRAW);

	this->VertArray.Unbind();
	this->VertBuffer.Unbind();
	this->ElementBuffer.Unbind();
}

unsigned int Object_ParticleEmitter::m_getUsableIndex()
{
	if (this->m_particles.size() == 0) //Are there any particles at all?
	{
		this->m_particles.resize(1); //Resize array to length 1

		return 0;
	}
	else
	{
		if ((!this->m_particles[0]) || (this->m_particles[0]->AliveTime > this->Lifetime))
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

				if (this->m_particles[Index]->AliveTime > this->Lifetime)
				{ //Get first inactive particle
					delete this->m_particles[Index];

					this->m_particles[Index] = nullptr;

					return Index;
				}
			}
		}
	}

	if (this->m_particles.size() < this->MaxParticles) {
		this->m_particles.resize(this->m_particles.size() + 1);

		return this->m_particles.size() - 1;
	}

	return 0;
}

void Object_ParticleEmitter::Update(double Delta)
{
	if (!DidInit)
	{
		if (!Object_ParticleEmitter::ParticleShaders)
			Object_ParticleEmitter::ParticleShaders = new ShaderProgram("particle.vert", "particle.frag");

		this->VertArray.Bind();

		this->VertArray.LinkAttrib(this->VertBuffer, 0, 2, GL_FLOAT, 2 * sizeof(float), (void*)0);

		this->VertBuffer.Bind();
		glBufferData(GL_ARRAY_BUFFER, sizeof(Quad), &Quad, GL_STREAM_DRAW);

		GLuint quadinds[] = {
			0, 1, 2,
			2, 3, 0
		};

		this->ElementBuffer.Bind();
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadinds), &quadinds, GL_STREAM_DRAW);

		this->VertArray.Unbind();
		this->VertBuffer.Unbind();
		this->ElementBuffer.Unbind();

		DidInit = true;
	}

	float TimeBetweenSpawn = 1.0f / this->Rate;

	std::shared_ptr<Object_Base3D> Parent3D = std::dynamic_pointer_cast<Object_Base3D>(this->Parent);

	Vector3 WorldPosition = Parent3D ? Vector3(glm::vec3(Parent3D->Matrix[3])) + this->Offset : this->Offset;

	_particle* NewParticle = nullptr;

	if (this->m_lastSpawnedDelta >= TimeBetweenSpawn && this->PossibleImages.size() > 0)
	{
		this->m_lastSpawnedDelta = 0.0f;

		//Spawn a new particle

		float numimages = (float)this->PossibleImages.size();

		std::uniform_real_distribution<double> rand_idx(0, numimages);

		NewParticle = new _particle();
		NewParticle->Image = this->PossibleImages[(unsigned int)rand_idx(Object_ParticleEmitter::rand_generator)];
		NewParticle->Position = WorldPosition;

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
			Particle->AliveTime = Particle->AliveTime + Delta;

			float AlivePercent = Particle->AliveTime / this->Lifetime;

			if (Particle->AliveTime <= this->Lifetime)
			{
				Vector3 CurrentMaxRandDeviation = this->RandDeviationOverTime.GetValue(AlivePercent);

				std::uniform_real_distribution<double> xrand(-CurrentMaxRandDeviation.X / 2, CurrentMaxRandDeviation.X / 2);
				std::uniform_real_distribution<double> yrand(-CurrentMaxRandDeviation.Y / 2, CurrentMaxRandDeviation.Y / 2);
				std::uniform_real_distribution<double> zrand(-CurrentMaxRandDeviation.Z / 2, CurrentMaxRandDeviation.Z / 2);

				Vector3 Rand = Vector3(
					xrand(Object_ParticleEmitter::rand_generator),
					yrand(Object_ParticleEmitter::rand_generator),
					zrand(Object_ParticleEmitter::rand_generator)
				);

				Particle->Position = Particle->Position + (this->VelocityOverTime.GetValue(AlivePercent) * Delta) + Rand;
				Particle->Size = this->SizeOverTime.GetValue(AlivePercent);

				Particle->Transparency = this->TransparencyOverTime.GetValue(AlivePercent);
			}
		}
	}
}

void Object_ParticleEmitter::Render(Camera SceneCamera)
{
	if (this->m_particles.size() == 0)
		return;

	this->VertArray.Bind();
	this->VertBuffer.Bind();
	this->ElementBuffer.Bind();

	Object_ParticleEmitter::ParticleShaders->Activate();

	glEnable(GL_BLEND);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	for (int Index = 0; Index < this->m_particles.size(); Index++)
	{
		_particle* Particle = this->m_particles[Index];

		if (Particle == nullptr || Particle->AliveTime > this->Lifetime)
		{
			delete this->m_particles[Index];
			this->m_particles[Index] = nullptr;

			continue;
		}

		Object_ParticleEmitter::ParticleShaders->Activate();

		glUniform3f(
			glGetUniformLocation(Object_ParticleEmitter::ParticleShaders->ID, "Position"),
			Particle->Position.X,
			Particle->Position.Y,
			Particle->Position.Z
		);

		glUniform1f(
			glGetUniformLocation(Object_ParticleEmitter::ParticleShaders->ID, "Transparency"),
			Particle->Transparency
		);

		glm::mat4 Scale = glm::mat4(1.0f);

		Scale = glm::scale(Scale, glm::vec3(Particle->Size, Particle->Size, Particle->Size));

		glUniformMatrix4fv(
			glGetUniformLocation(Object_ParticleEmitter::ParticleShaders->ID, "Scale"),
			1,
			GL_FALSE,
			glm::value_ptr(Scale)
		);

		glUniformMatrix4fv(
			glGetUniformLocation(Object_ParticleEmitter::ParticleShaders->ID, "CameraMatrix"),
			1,
			GL_FALSE,
			glm::value_ptr(SceneCamera.Matrix)
		);

		glUniform3f(
			glGetUniformLocation(Object_ParticleEmitter::ParticleShaders->ID, "CameraPosition"),
			SceneCamera.Position.x,
			SceneCamera.Position.y,
			SceneCamera.Position.z
		);

		glBindTexture(GL_TEXTURE_2D, Particle->Image->Identifier);

		GLint ActiveID;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &ActiveID);

		glUniform1i(glGetUniformLocation(Object_ParticleEmitter::ParticleShaders->ID, "Image"), ActiveID);

		glDrawElements(GL_TRIANGLES, 7, GL_UNSIGNED_INT, 0);
	}

	this->VertArray.Unbind();
	this->VertBuffer.Unbind();
	this->ElementBuffer.Unbind();

	glDisable(GL_BLEND);
}
