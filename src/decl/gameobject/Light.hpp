#pragma once

#include "gameobject/Attachment.hpp"
#include "datatype/Color.hpp"

class Object_Light : public Object_Attachment
{
public:
	Object_Light();

	Color LightColor = Color(1.0f, 1.0f, 1.0f);
	float Brightness = 1.0f;
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

	float OuterCone = 90.f;
	float InnerCone = 95.f;

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};
