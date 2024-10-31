#pragma once

#include <glm/matrix.hpp>

#include "datatype/GameObject.hpp"

#include "datatype/Color.hpp"
#include "datatype/Vector3.hpp"
#include "asset/Material.hpp"

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

	Vector3 LinearVelocity;
	Vector3 AngularVelocity;
	double Mass = 1.f;
	double Density = 1.f;
	double Friction = 0.3f;

	float Transparency = 0.f;
	float Reflectivity = 0.f;

	Color ColorRGB = Color(1.f, 1.f, 1.f);

	RenderMaterial* Material;

	FaceCullingMode FaceCulling = FaceCullingMode::BackFace;

	PHX_GAMEOBJECT_API_REFLECTION;

protected:
	uint32_t m_RenderMeshId{};

private:
	static void s_DeclareReflections();
	static inline Api s_Api{};
};
