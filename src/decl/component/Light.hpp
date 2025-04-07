#pragma once

#include "datatype/GameObject.hpp"
#include "datatype/Color.hpp"

struct EcLight
{
	Color LightColor{ 1.f, 1.f, 1.f };
	float Brightness = 1.f;
	bool Shadows = false;
};

struct EcPointLight : public EcLight
{
	float Range = 16.f;
	
	static inline EntityComponent Type = EntityComponent::PointLight;
};

struct EcDirectionalLight : public EcLight
{
	GameObjectRef Object;

	static inline EntityComponent Type = EntityComponent::DirectionalLight;
};

struct EcSpotLight : public EcLight
{
	float Range = 16.f;
	float Angle = 45.f;

	static inline EntityComponent Type = EntityComponent::SpotLight;
};

/*
class Object_Light : public Object_Attachment
{
public:
	Object_Light();

	Color LightColor{ 1.f, 1.f, 1.f };
	float Brightness = 1.f;
	bool Shadows = false;

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};

class Object_PointLight : public Object_Light
{
public:
	Object_PointLight();

	float Range = 16.f;

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};

class Object_DirectionalLight : public Object_Light
{
public:
	Object_DirectionalLight();

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};

class Object_SpotLight : public Object_Light
{
public:
	Object_SpotLight();

	float Range = 16.f;
	float Angle = 45.f;

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};
*/
