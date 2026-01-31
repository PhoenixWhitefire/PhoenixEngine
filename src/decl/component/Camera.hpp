#pragma once

#include <glm/mat4x4.hpp>

#include "datatype/GameObject.hpp"
#include "datatype/ComponentBase.hpp"

struct EcCamera : public Component<EntityComponent::Camera>
{
	// Given the camera's current position and rotation,
	// get it's Matrix accounting for Projection (FoV, AspectRatio, NearPlane and FarPlane)
	// @param The Aspect Ratio
	glm::mat4 GetRenderMatrix(float AspectRatio) const;

	glm::mat4 GetWorldTransform() const;
	void SetWorldTransform(const glm::mat4&);

	// if the Camera is the Scene Camera, the engine will move it with
	// WASD + Mouse
	bool UseSimpleController = false;
	bool IsSceneCamera = false;

	float FieldOfView = 90.f;
	float NearPlane = 0.1f;
	float FarPlane = 10000.f;

	// When in `UseSimpleController` mode (not functional yet)
	float MovementSpeed = 0.5f;
	float MouseSensitivity = 100.f;
	bool Valid = true;

	ObjectRef Object;
};
