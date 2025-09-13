#pragma once

#include <string>
#include <glm/vec3.hpp>

#include "datatype/GameObject.hpp"
#include "datatype/Color.hpp"

enum class FaceCullingMode : uint8_t { None, BackFace, FrontFace };
enum class CollisionFidelityMode : uint8_t { Aabb, AabbStaticSize };

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

	glm::vec3 LinearVelocity;
	glm::vec3 AngularVelocity;
	struct
	{
		glm::vec3 Position{};
		glm::vec3 Size{ 1.f, 1.f, 1.f };
	} CollisionAabb;
	CollisionFidelityMode CollisionFidelity = CollisionFidelityMode::Aabb;

	double Mass = 1.f;
	double Density = 1.f;
	double Friction = 0.3f;

	std::string Asset = "!Cube";
	uint32_t GpuSkinningBuffer = UINT32_MAX;

	GameObjectRef Object;
	bool Valid = true;

	static inline EntityComponent Type = EntityComponent::Mesh;
};
