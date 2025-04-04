#pragma once

#include "render/RendererScene.hpp"

#include "datatype/GameObject.hpp"
#include "datatype/ValueSequence.hpp"
#include "datatype/Vector3.hpp"
#include "datatype/Vector2.hpp"

struct EcParticleEmitter
{
	std::vector<RenderItem> GetRenderList() { return {}; };

	bool Emitting = true;
	bool ParticlesRelativeToEmitter = false;

	uint32_t Rate = 50; //Particles to be spawned every second
	Vector2 Lifetime = Vector2(1.5f, 2.f); // Randomly chosen between the range X - Y;

	std::vector<uint32_t> PossibleImages; //A random one is chosen every time a particle needs to be spawned

	ValueSequence<float> TransparencyOverTime;
	ValueSequence<float> SizeOverTime;
	ValueSequence<glm::vec3> VelocityOverTime;

	static inline EntityComponent Type = EntityComponent::ParticleEmitter;
};

#if 0

#include "gameobject/Attachment.hpp"

#include "render/RendererScene.hpp"

#include "datatype/ValueSequence.hpp"
#include "datatype/Vector3.hpp"
#include "datatype/Vector2.hpp"

class Object_ParticleEmitter : public Object_Attachment
{
public:
	Object_ParticleEmitter();

	void Update(double Delta) override;
	std::vector<RenderItem> GetRenderList();

	bool EmitterEnabled = true;
	bool ParticlesRelativeToEmitter = false;

	uint32_t Rate = 50; //Particles to be spawned every second
	//uint32_t MaxParticles = 500;

	Vector2 Lifetime = Vector2(1.5f, 2.f); // Randomly chosen between the range X - Y;

	std::vector<uint32_t> PossibleImages; //A random one is chosen every time a particle needs to be spawned

	ValueSequence<float> TransparencyOverTime;
	ValueSequence<float> SizeOverTime;
	ValueSequence<glm::vec3> VelocityOverTime;
	
	REFLECTION_DECLAREAPI;

private:
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

	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};

#endif
