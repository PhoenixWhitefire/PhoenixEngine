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

	PHX_GAMEOBJECT_API_REFLECTION;

private:
	static void s_DeclareReflections();
	static inline Api s_Api{};
};

class Object_PointLight : public Object_Light
{
public:
	Object_PointLight();

	float Range = 16.f;

	PHX_GAMEOBJECT_API_REFLECTION;

private:
	static void s_DeclareReflections();
	static inline Api s_Api{};
};

class Object_DirectionalLight : public Object_Light
{
public:
	Object_DirectionalLight();

	PHX_GAMEOBJECT_API_REFLECTION;

private:
	static void s_DeclareReflections();
	static inline Api s_Api{};
};

class Object_SpotLight : public Object_Light
{
public:
	Object_SpotLight();

	float Range = 16.f;

	float OuterCone = 90.f;
	float InnerCone = 95.f;

	PHX_GAMEOBJECT_API_REFLECTION;

private:
	static void s_DeclareReflections();
	static inline Api s_Api{};
};
