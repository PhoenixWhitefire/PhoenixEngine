#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

#include"render/Camera.hpp"
#include"Debug.hpp"

Camera::Camera(Vector2 WindowSize)
{
	this->WindowWidth = WindowSize.X;
	this->WindowHeight = WindowSize.Y;
}

#include<format>

void Camera::Update()
{
	float AspectRatio = (float)this->WindowWidth / (float)this->WindowHeight;

	if (isnan(AspectRatio))
	{
		Debug::Log(std::vformat("Camera had NaN aspect ratio {}/{}!", std::make_format_args(WindowWidth, WindowHeight)));
		return;
	}

	glm::mat4 ViewMatrix = glm::mat4(1.0f);
	glm::mat4 ProjectionMatrix = glm::mat4(1.0f);

	ViewMatrix = glm::lookAt(this->Position, this->Position + this->LookVec, (glm::vec3)Vector3::UP);
	ProjectionMatrix = glm::perspective(glm::radians(this->FOV), AspectRatio, this->NearPlane, this->FarPlane);

	this->Matrix = ProjectionMatrix * ViewMatrix;

	// fish out the upvector from the generated matrix
	//this->UpVec = glm::vec3(this->Matrix[0][1], this->Matrix[1][1], this->Matrix[2][1]);
}
