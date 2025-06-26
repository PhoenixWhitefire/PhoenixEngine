#pragma once

#include "component/Mesh.hpp"
#include "Memory.hpp"

namespace Physics
{
	void Step(Memory::vector<GameObject*, MEMCAT(Physics)>& World, double DeltaTime);
}
