#pragma once

#include "component/Mesh.hpp"
#include "Memory.hpp"

class Physics
{
public:
	static Physics* Get();
	Physics();
	~Physics();

	struct World
	{
		std::vector<ObjectHandle> Dynamics;
		std::vector<ObjectHandle> Statics;
	};

	void Step(World& World, double DeltaTime);

	glm::vec3 Gravity = { 0.f, -50.f, 0.f };

	// Handled externally
	double Timescale = 1.f;
	bool Simulating = true;
	bool DebugCollisionAabbs = false;
	bool DebugContactPoints = false;
	bool DebugSpatialHeat = false;
};
