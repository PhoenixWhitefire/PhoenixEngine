#include<glm/gtc/matrix_transform.hpp>

#include"PhysicsEngine.hpp"

void PhysicsSolver::ComputePhysicsForObject(std::shared_ptr<Object_Base3D> Object, double Delta)
{
	if (Object->ComputePhysics)
	{
		// base "collision" and then teleport test
		// TODO: implement collisions
		if (glm::vec3(Object->Matrix[3]).y <= 0)
		{
			Object->Matrix = glm::translate(Object->Matrix, glm::vec3(0.0f, 150.0f, 0.0f));
			return;
		}

		Vector3 GravityForce = this->WorldGravity * Object->Mass;

		Object->LinearVelocity = Object->LinearVelocity + (GravityForce / Object->Mass * Delta);

		Object->Matrix = glm::translate(Object->Matrix, glm::vec3(Object->LinearVelocity * Delta));
	}
}
