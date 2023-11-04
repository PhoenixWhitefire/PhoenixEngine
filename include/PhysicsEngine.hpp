#pragma once

#include<datatype/GameObject.hpp>
#include<gameobject/Base3D.hpp>

#include<datatype/Vector3.hpp>
#include<glm/gtc/matrix_transform.hpp>

class PhysicsSolver
{
public:
	void ComputePhysicsForObject(std::shared_ptr<Object_Base3D> Object, double Delta);

	Vector3 WorldGravity = Vector3(0.0f, -0.1f, 0.0f);
};