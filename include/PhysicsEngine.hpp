#pragma once

#include"gameobject/Base3D.hpp"
#include<datatype/Vector3.hpp>

class PhysicsSolver
{
public:
	void ComputePhysicsForObject(std::shared_ptr<Object_Base3D> Object, double Delta);

	Vector3 WorldGravity = Vector3(0.0f, -0.1f, 0.0f);
};