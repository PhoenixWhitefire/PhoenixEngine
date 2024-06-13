#pragma once

#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

#include"ShaderProgram.hpp"
#include<datatype/Vector3.hpp>
#include<datatype/Vector2.hpp>

class Camera {
public:
	Camera(Vector2 WindowSize);

	// Update matrix for shaders
	void Update();

	glm::vec3 Position;
	glm::vec3 LookVec = Vector3::FORWARD;
	glm::vec3 UpVec = Vector3::UP;

	int WindowWidth;
	int WindowHeight;

	float NearPlane = 0.1f;
	float FarPlane = 1000.0f;

	float FOV = 45.0f;

	glm::mat4 Matrix = glm::mat4(1.0f);
};
