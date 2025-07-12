#pragma once

#include "component/Mesh.hpp"
#include "Memory.hpp"

namespace Physics
{
	void Step(Memory::vector<GameObjectRef, MEMCAT(Physics)>& World, double DeltaTime);
}
