#pragma once

#include<random>

#include"datatype/ValueRange.hpp"
#include"datatype/Buffer.hpp"
#include"datatype/GameObject.hpp"
#include"render/ShaderProgram.hpp"
#include"render/TextureManager.hpp"
#include"datatype/Vector2.hpp"

float Quad[];

struct _particle
{
public:
	double Lifetime;
	double TimeAliveFor;
	float Size;
	float Transparency;

	Texture* Image = nullptr;

	Vector3 Position;
	
};

class Object_ParticleEmitter : public GameObject
{
public:
	Object_ParticleEmitter();

	void Update(double Delta);
	void Render(glm::mat4 CameraMatrix);

	bool EmitterEnabled = true;

	float Rate = 50.0f; //Particles to be spawned every second
	int MaxParticles = 500;

	Vector2 Lifetime = Vector2(1.5f, 2.f); // Randomly chosen between the range X - Y;
	Vector3 Offset; // If parent is a valid Base3D, position will be Parent.Position + Offset, else just Offset

	std::vector<Texture*> PossibleImages; //A random one is chosen every time a particle needs to be spawned

	ValueRange<float> TransparencyOverTime;
	ValueRange<float> SizeOverTime;
	ValueRange<Vector3> VelocityOverTime;
	
private:
	uint32_t m_getUsableIndex();

	std::vector<_particle*> m_particles;

	double m_lastSpawnedDelta = 0.f;

	VAO VertArray;
	VBO VertBuffer;
	EBO ElementBuffer;

	static std::default_random_engine s_RandGenerator;
	static ShaderProgram* s_ParticleShaders;

	static DerivedObjectRegister<Object_ParticleEmitter> RegisterClassAs;
};
