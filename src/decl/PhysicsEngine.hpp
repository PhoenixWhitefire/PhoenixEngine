#pragma once

#include "gameobject/Base3D.hpp"

namespace Physics
{
	void Step(std::vector<Object_Base3D*>& World, double DeltaTime);
}
