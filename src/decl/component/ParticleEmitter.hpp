#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include "render/RendererScene.hpp"

#include "datatype/GameObject.hpp"
#include "datatype/ValueGradient.hpp"

struct EcParticleEmitter
{
	EcParticleEmitter();

	void Update(double);
	void AppendToRenderList(std::vector<RenderItem>&);

	GameObjectRef Object;

	bool Emitting = true;
	bool ParticlesAreAttached = false;

	uint32_t Rate = 50; //Particles to be spawned every second
	glm::vec2 Lifetime{ 1.5f, 2.f }; // Randomly chosen between the range X - Y;

	std::vector<uint32_t> PossibleImages; //A random one is chosen every time a particle needs to be spawned

	ValueGradient<float> TransparencyOverTime;
	ValueGradient<float> SizeOverTime;
	ValueGradient<glm::vec3> VelocityOverTime;

	static inline EntityComponent Type = EntityComponent::ParticleEmitter;

	struct Particle
	{
		float Lifetime = 1.f;
		float TimeAliveFor = 0.f;
		float Size = 1.f;
		float Transparency = 0.f;

		uint32_t Image{};

		glm::vec3 Position{};
	};

	size_t m_GetUsableParticleIndex();

	std::vector<Particle> m_Particles;

	double m_TimeSinceLastSpawn = 0.f;
	bool Valid = true;
};
