#pragma once

#include "datatype/GameObject.hpp"
#include "datatype/Color.hpp"

template <EntityComponent T>
struct EcLight : Component<T>
{
	Color LightColor{ 1.f, 1.f, 1.f };
	float Brightness = 1.f;
	bool Shadows = false;
	bool Valid = true;
};

struct EcPointLight : public EcLight<EntityComponent::PointLight>
{
	float Range = 16.f;
};

struct EcDirectionalLight : public EcLight<EntityComponent::DirectionalLight>
{
	glm::vec3 ShadowViewOffset;
	float ShadowViewDistance = 200.f;
	float ShadowViewSizeH = 100.f;
	float ShadowViewSizeV = 100.f;
	float ShadowViewFarPlane = 700.f;
	float ShadowViewNearPlane = 0.1f;
	bool ShadowViewMoveWithCamera = true;

	ObjectRef Object;
};

struct EcSpotLight : public EcLight<EntityComponent::SpotLight>
{
	float Range = 16.f;
	float Angle = 45.f;
};
