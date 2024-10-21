#pragma once

#include <random>

#include "datatype/ValueSequence.hpp"
#include "render/GpuBuffers.hpp"
#include "datatype/GameObject.hpp"
#include "render/ShaderProgram.hpp"
#include "asset/TextureManager.hpp"
#include "datatype/Vector3.hpp"
#include "datatype/Vector2.hpp"

float Quad[];

struct _particle
{
	float Lifetime = 1.f;
	float TimeAliveFor = 0.f;
	float Size = 1.f;
	float Transparency = 0.f;

	uint32_t Image{};

	Vector3 Position{};
};

class Object_ParticleEmitter : public GameObject
{
public:
	Object_ParticleEmitter();

	void Update(double Delta) override;
	void Render(glm::mat4 CameraTransform);

	bool EmitterEnabled = true;

	uint32_t Rate = 50; //Particles to be spawned every second
	uint32_t MaxParticles = 500;

	Vector2 Lifetime = Vector2(1.5f, 2.f); // Randomly chosen between the range X - Y;
	Vector3 Offset; // If parent is a valid Base3D, position will be Parent.Position + Offset, else just Offset

	std::vector<uint32_t> PossibleImages; //A random one is chosen every time a particle needs to be spawned

	ValueSequence<float> TransparencyOverTime;
	ValueSequence<float> SizeOverTime;
	ValueSequence<Vector3> VelocityOverTime;
	
	PHX_GAMEOBJECT_API_REFLECTION;

private:
	size_t m_GetUsableParticleIndex();

	std::vector<_particle> m_Particles;

	double m_TimeSinceLastSpawn = 0.f;

	GpuVertexArray m_VertArray;
	GpuVertexBuffer m_VertBuffer;
	GpuElementBuffer m_ElementBuffer;

	static void s_DeclareReflections();
	static inline Api s_Api{};
};
