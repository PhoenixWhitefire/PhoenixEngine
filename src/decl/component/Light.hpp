#pragma once

#include "datatype/GameObject.hpp"
#include "datatype/Color.hpp"

struct EcLight
{
	Color LightColor{ 1.f, 1.f, 1.f };
	float Brightness = 1.f;
	bool Shadows = false;
	bool Valid = true;
};

struct EcPointLight : public EcLight
{
	float Range = 16.f;
	
	static inline EntityComponent Type = EntityComponent::PointLight;
};

struct EcDirectionalLight : public EcLight
{
	glm::vec3 ShadowViewOffset;
	float ShadowViewDistance = 300.f;
	float ShadowViewSizeH = 200.f;
	float ShadowViewSizeV = 200.f;
	float ShadowViewFarPlane = 700.f;
	float ShadowViewNearPlane = 0.1f;
	bool ShadowViewMoveWithCamera = true;

	ObjectRef Object;

	static inline EntityComponent Type = EntityComponent::DirectionalLight;
};

struct EcSpotLight : public EcLight
{
	float Range = 16.f;
	float Angle = 45.f;

	static inline EntityComponent Type = EntityComponent::SpotLight;
};
