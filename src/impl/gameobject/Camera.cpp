#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <tracy/Tracy.hpp> 

#include "gameobject/Camera.hpp"
#include "datatype/Vector3.hpp"
#include "UserInput.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Camera);

static bool s_DidInitReflection = false;

void Object_Camera::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROP_SIMPLE(Object_Camera, UseSimpleController, Bool);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Camera, FieldOfView, Double, float);

	REFLECTION_DECLAREPROP_SIMPLE(Object_Camera, Transform, Matrix);
}

Object_Camera::Object_Camera()
{
	this->Name = "Camera";
	this->ClassName = "Camera";

	this->Transform = glm::lookAt(glm::vec3(), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, 1.f, 0.f));

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

void Object_Camera::Update(double)
{
	if (!this->IsSceneCamera)
		return;

	if (!this->UseSimpleController)
		return;

	// Camera movement code
}

glm::mat4 Object_Camera::GetMatrixForAspectRatio(float AspectRatio) const
{
	glm::vec3 position = glm::vec3(this->Transform[3]);
	glm::vec3 forwardVec = glm::vec3(this->Transform[2]);

	glm::mat4 projectionMatrix = glm::perspective(
		glm::radians(this->FieldOfView),
		AspectRatio,
		this->NearPlane,
		this->FarPlane
	);
	glm::mat4 viewMatrix = glm::lookAt(
		position,
		position + forwardVec,
		glm::vec3(this->Transform[1])
	);

	return projectionMatrix * viewMatrix;
}
