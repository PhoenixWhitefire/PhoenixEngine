#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include "render/RendererScene.hpp"

#include "datatype/GameObject.hpp"
#include "datatype/ValueGradient.hpp"

struct EcParticleEmitter : public Component<EntityComponent::ParticleEmitter>
{
	EcParticleEmitter();

	void Update(double);
	void Render(const glm::mat4& RenderMatrix);

	ObjectRef Object;

	uint32_t Rate = 50; // Particles to be spawned every second
	glm::vec2 Lifetime = { 1.5f, 2.f }; // Randomly chosen between the range X - Y;

	uint32_t Image = 1;
	ValueGradient<float> TransparencyOverTime;
	ValueGradient<float> SizeOverTime;
	ValueGradient<glm::vec3> VelocityOverTime;
	ValueGradient<Color> ColorOverTime;

	struct Particle
	{
		float Lifetime = 1.f;
		float TimeAliveFor = 0.f;
		float Size = 1.f;
		float Transparency = 0.f;
		Color Tint = { 1.f, 1.f, 1.f };

		glm::vec3 Position;
	};

	size_t m_GetUsableParticleIndex();

	std::vector<Particle> m_Particles;

	double m_TimeSinceLastSpawn = 0.f;
	bool Emitting = true;
	bool ParticlesAreAttached = false;
	bool Valid = true;
};

class ParticleEmitterComponentManager : public ComponentManager<EcParticleEmitter>
{
public:
    uint32_t CreateComponent(GameObject* Object) override;
};
