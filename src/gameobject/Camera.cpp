#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

#include"gameobject/Camera.hpp"
#include"UserInput.hpp"

DerivedObjectRegister<Object_Camera> Object_Camera::RegisterClassAs("Camera");

Object_Camera::Object_Camera()
{
	this->Name = "Camera";
	this->ClassName = "Camera";

	PrevMouseX = 0;
	PrevMouseY = 0;

	REFLECTION_DECLAREPROP_SIMPLE(GenericMovement, Reflection::ValueType::Bool);
	REFLECTION_DECLAREPROP_SIMPLE(FieldOfView, Reflection::ValueType::Double);

	REFLECTION_DECLAREPROP(
		Position,
		Reflection::ValueType::Vector3,
		[this]()
		{
			Vector3 v = Vector3((glm::vec3)this->Matrix[3]);
			return v;
		},
		[this](Reflection::GenericValue gv)
		{
			Vector3& vec = *(Vector3*)gv.Pointer;
			this->Matrix[3] = glm::vec4(vec.X, vec.Y, vec.Z, 1.f);
		}
	);
}

void Object_Camera::Update(double DeltaTime)
{
	if (!this->IsSceneCamera)
		return;

	if (!this->GenericMovement)
		return;
}

glm::mat4 Object_Camera::GetMatrixForAspectRatio(double AspectRatio) const
{
	glm::mat4 ViewMatrix = glm::mat4(1.0f);
	glm::mat4 ProjectionMatrix = glm::mat4(1.0f);

	glm::vec3 Position = glm::vec3(this->Matrix[3]);
	glm::vec3 ForwardVec = glm::vec3(this->Matrix[2]);

	ViewMatrix = glm::lookAt(Position, Position + ForwardVec, (glm::vec3)Vector3::UP);
	ProjectionMatrix = glm::perspective(glm::radians(this->FieldOfView), AspectRatio, this->NearPlane, this->FarPlane);

	return ProjectionMatrix * ViewMatrix;
}
