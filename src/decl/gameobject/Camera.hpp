#pragma once

#include <glm/matrix.hpp>

#include "datatype/GameObject.hpp"

class Object_Camera : public GameObject
{
public:
	Object_Camera();

	void Update(double) override;
	// Given the camera's current position and rotation,
	// get it's Matrix accounting for Projection (FoV, AspectRatio, NearPlane and FarPlane)
	// @param The Aspect Ratio
	glm::mat4 GetMatrixForAspectRatio(float) const;

	// if the Camera is the Scene Camera, the engine will move it with
	// WASD + Mouse
	bool UseSimpleController = true;
	bool IsSceneCamera = false;

	glm::mat4 Transform = glm::mat4(1.0f);
	float FieldOfView = 70.f;
	float NearPlane = 0.1f;
	float FarPlane = 1000.f;

	// When in GenericMovement mode
	float MovementSpeed = 0.5f;
	float MouseSensitivity = 100.f;

	REFLECTION_DECLAREAPI;
	
private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};
