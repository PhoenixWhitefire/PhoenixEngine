#pragma once

#include<glm/matrix.hpp>

#include"datatype/GameObject.hpp"

class Object_Camera : public GameObject
{
public:
	Object_Camera();
	void Update(double);
	// Given the camera's current position and rotation,
	// get it's Matrix accounting for Projection (FoV, AspectRatio, NearPlane and FarPlane)
	// @param The Aspect Ratio
	glm::mat4 GetMatrixForAspectRatio(double) const;

	// if the Camera is the Scene Camera, the engine will move it with
	// WASD + Mouse
	bool GenericMovement = true;
	bool IsSceneCamera = false;

	glm::mat4 Matrix = glm::mat4(1.0f);
	double FieldOfView = 70.f;
	double NearPlane = 0.1f;
	double FarPlane = 1000.f;

	// When in GenericMovement mode
	float MovementSpeed = 0.5f;
	float MouseSensitivity = 100.f;
	
private:
	static DerivedObjectRegister<Object_Camera> RegisterClassAs;

	// When in GenericMovement mode
	bool FirstDragFrame = false;
	int PrevMouseX;
	int PrevMouseY;
};
