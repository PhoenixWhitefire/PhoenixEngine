#pragma once

#include <glm/matrix.hpp>

#include "datatype/GameObject.hpp"

#include "datatype/Vector3.hpp"
#include "datatype/Color.hpp"

enum class FaceCullingMode : uint8_t { None, BackFace, FrontFace };

class Object_Base3D : public GameObject
{
public:
	Object_Base3D();

	void RecomputeAabb();

	glm::mat4 Transform = glm::mat4(1.f);
	Vector3 Size = Vector3(1.f, 1.f, 1.f);

	bool PhysicsDynamics = false;
	bool PhysicsCollisions = true;
	bool CastsShadows = true;

	FaceCullingMode FaceCulling = FaceCullingMode::BackFace;

	uint32_t RenderMeshId{};
	uint32_t MaterialId{};

	float Transparency = 0.f;
	float MetallnessFactor = 1.f;
	float RoughnessFactor = 1.f;

	Color Tint = Color(1.f, 1.f, 1.f);

	Vector3 LinearVelocity;
	Vector3 AngularVelocity;
	struct
	{
		Vector3 Position{};
		Vector3 Size{ 1.f, 1.f, 1.f };
	} CollisionAabb;

	double Mass = 1.f;
	double Density = 1.f;
	double Friction = 0.3f;

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};
