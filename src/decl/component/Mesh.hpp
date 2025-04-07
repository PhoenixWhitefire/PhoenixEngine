#pragma once

#include <string>

#include "datatype/GameObject.hpp"

#include "datatype/Vector3.hpp"
#include "datatype/Color.hpp"

enum class FaceCullingMode : uint8_t { None, BackFace, FrontFace };

struct EcMesh
{
	void RecomputeAabb();
	void SetRenderMesh(const std::string_view&);

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

	std::string Asset = "!Cube";
	bool WaitingForMeshToLoad = false;

	GameObjectRef Object;

	static inline EntityComponent Type = EntityComponent::Mesh;
};

/*
class Object_Mesh : public Object_Base3D
{
public:
	Object_Mesh();
	void Update(double) override;

	std::string GetRenderMeshPath();
	void SetRenderMesh(const std::string_view&);

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;

	std::string m_MeshPath{};
	bool m_WaitingForMeshToLoad = false;
};
*/
