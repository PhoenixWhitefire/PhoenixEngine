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

	virtual uint32_t GetRenderMeshId();

	glm::mat4 Transform = glm::mat4(1.f);
	Vector3 Size = Vector3(1.f, 1.f, 1.f);

	bool PhysicsDynamics = false;
	bool PhysicsCollisions = true;

	FaceCullingMode FaceCulling = FaceCullingMode::BackFace;

	float Transparency = 0.f;
	float Reflectivity = 0.f;

	Color ColorRGB = Color(1.f, 1.f, 1.f);

	uint32_t MaterialId{};

	Vector3 LinearVelocity;
	Vector3 AngularVelocity;
	double Mass = 1.f;
	double Density = 1.f;
	double Friction = 0.3f;

	REFLECTION_DECLAREAPI;

protected:
	uint32_t m_RenderMeshId{};

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};
