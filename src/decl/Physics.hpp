#pragma once

#include "component/Mesh.hpp"
#include "Memory.hpp"

struct PhysicsWorld
{
	std::vector<ObjectHandle> Dynamics;
	std::vector<ObjectHandle> Statics;
};

namespace Physics
{
	void Step(PhysicsWorld& World, double DeltaTime);
}
