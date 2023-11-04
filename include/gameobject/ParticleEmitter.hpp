#pragma once

#include<random>

#include<render/TextureManager.hpp>
#include<render/ShaderProgram.hpp>
#include<datatype/ValueRange.hpp>
#include<datatype/Buffer.hpp>
#include<render/Camera.hpp>

#include<datatype/GameObject.hpp>

float Quad[];

class _particle
{
public:
	double AliveTime = 0.0f;

	Texture* Image = nullptr;

	Vector3 Position; //World position
	float Size = 1.0f; //1x1x1

	float Transparency = 0.0f;
};

class Object_ParticleEmitter : public GameObject
{
public:
	Object_ParticleEmitter();

	bool DidInit = false;

	void Update(double Delta);
	void Render(Camera SceneCamera);

	static ShaderProgram* ParticleShaders;

	Vector3 Offset; //If parent is a valid Base3D, position will be Parent.Position + Offset, else just Offset
	ValueRange<Vector3> VelocityOverTime;
	ValueRange<Vector3> RandDeviationOverTime;

	int MaxParticles = 500;

	bool Enabled = true;

	std::vector<Texture*> PossibleImages; //A random one is chosen every time a particle needs to be spawned

	float Lifetime = 3.0f;

	ValueRange<float> TransparencyOverTime;
	ValueRange<float> SizeOverTime;

	float Rate = 50.0f; //Particles to be spawned every second

	static std::default_random_engine rand_generator;

	static DerivedObjectRegister<Object_ParticleEmitter> RegisterClassAs;

private:
	unsigned int m_getUsableIndex();

	std::vector<_particle*> m_particles;

	double m_lastSpawnedDelta = 0.f;

	VAO VertArray;
	VBO VertBuffer;
	EBO ElementBuffer;
};
