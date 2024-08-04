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

	m_properties.insert(std::pair(
		"GenericMovement",
		PropInfo
		{
		PropType::Bool,
		PropReflection
		{
			[this]()
			{
				return this->GenericMovement;
			},

			[this](GenericType gt)
			{
				this->GenericMovement = gt.Bool;
			}
		}
		}
	));
	m_properties.insert(std::pair(
		"FieldOfView",
		PropInfo
		{
			PropType::Double,
			PropReflection
			{
				[this]() { return this->FieldOfView; },
				[this](GenericType gt) { this->FieldOfView = gt.Double; }
			}
		}
	));
	m_properties.insert(std::pair(
		"Position",
		PropInfo
		{
		PropType::Vector3,
		PropReflection
		{
			[this]()
			{
				Vector3 pos = Vector3((glm::vec3)this->Matrix[3]);
				return pos;
			},

			[this](GenericType gt)
			{
				Vector3& vec = *(Vector3*)gt.Pointer;
				this->Matrix[3] = glm::vec4(vec.X, vec.Y, vec.Z, 1.f);
			}
		}
		}
	));
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
	ProjectionMatrix = glm::perspective(glm::radians(this->FieldOfView), (float)AspectRatio, this->NearPlane, this->FarPlane);

	return ProjectionMatrix * ViewMatrix;
}
