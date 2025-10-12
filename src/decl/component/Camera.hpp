#pragma once

#include <glm/mat4x4.hpp>

#include "datatype/GameObject.hpp"

struct EcCamera
{
	// Given the camera's current position and rotation,
	// get it's Matrix accounting for Projection (FoV, AspectRatio, NearPlane and FarPlane)
	// @param The Aspect Ratio
	glm::mat4 GetMatrixForAspectRatio(float) const;

	// if the Camera is the Scene Camera, the engine will move it with
	// WASD + Mouse
	bool UseSimpleController = false;
	bool IsSceneCamera = false;

	glm::mat4 Transform = glm::lookAt(glm::vec3(), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, 1.f, 0.f));
	float FieldOfView = 90.f;
	float NearPlane = 0.1f;
	float FarPlane = 10000.f;

	// When in `UseSimpleController` mode (not functional yet)
	float MovementSpeed = 0.5f;
	float MouseSensitivity = 100.f;
	bool Valid = true;

	static inline EntityComponent Type = EntityComponent::Camera;
};
