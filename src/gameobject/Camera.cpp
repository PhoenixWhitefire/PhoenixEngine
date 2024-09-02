#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

#include"gameobject/Camera.hpp"
#include"UserInput.hpp"

RegisterDerivedObject<Object_Camera> Object_Camera::RegisterClassAs("Camera");
static bool s_DidInitReflection = false;

void Object_Camera::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_DECLAREPROP_SIMPLE(Object_Camera, GenericMovement, Bool);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Camera, FieldOfView, Double);

	REFLECTION_DECLAREPROP(
		"Transform",
		Matrix,
		[](GameObject* p)
		{
			return Reflection::GenericValue(dynamic_cast<Object_Camera*>(p)->Transform);
		},
		[](GameObject* p, const Reflection::GenericValue& gv)
		{
			dynamic_cast<Object_Camera*>(p)->Transform = gv.AsMatrix();
		}
	);

	REFLECTION_INHERITAPI(GameObject);
}

Object_Camera::Object_Camera()
{
	this->Name = "Camera";
	this->ClassName = "Camera";

	PrevMouseX = 0;
	PrevMouseY = 0;

	this->Transform = glm::lookAt(glm::vec3(), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, 1.f, 0.f));

	s_DeclareReflections();
}

void Object_Camera::Update(double)
{
	if (!this->IsSceneCamera)
		return;

	if (!this->GenericMovement)
		return;

	// Camera movement code
}

glm::mat4 Object_Camera::GetMatrixForAspectRatio(double AspectRatio) const
{
	glm::mat4 ViewMatrix = glm::mat4(1.0f);
	glm::mat4 ProjectionMatrix = glm::mat4(1.0f);

	glm::vec3 Position = glm::vec3(this->Transform[3]);
	glm::vec3 ForwardVec = glm::vec3(this->Transform[2]);

	ViewMatrix = glm::lookAt(Position, Position + ForwardVec, (glm::vec3)Vector3::yAxis);
	ProjectionMatrix = glm::perspective(glm::radians(this->FieldOfView), AspectRatio, this->NearPlane, this->FarPlane);

	return ProjectionMatrix * ViewMatrix;
}
