#pragma once

#include "datatype/GameObject.hpp"
#include "datatype/Color.hpp"

template <EntityComponent T>
struct EcLight : Component<T>
{
	Color LightColor = { 1.f, 1.f, 1.f };
	float Brightness = 1.f;
	bool Valid = true;
};

struct EcPointLight : public EcLight<EntityComponent::PointLight>
{
	float Range = 16.f;
};

struct EcDirectionalLight : public EcLight<EntityComponent::DirectionalLight>
{
	glm::vec3 Direction = glm::vec3(0.5f, 0.5f, 0.5f);

	glm::vec3 ShadowViewOffset;
	float ShadowViewDistance = 200.f;
	float ShadowViewSizeH = 100.f;
	float ShadowViewSizeV = 100.f;
	float ShadowViewFarPlane = 700.f;
	float ShadowViewNearPlane = 0.1f;
	bool ShadowViewMoveWithCamera = true;
	bool Shadows = false;

	ObjectRef Object;
};

struct EcSpotLight : public EcLight<EntityComponent::SpotLight>
{
	float Range = 16.f;
	float Angle = 0.95f;
};
