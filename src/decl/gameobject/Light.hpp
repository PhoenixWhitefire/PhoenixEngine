#pragma once

#include"datatype/GameObject.hpp"

#include"datatype/Color.hpp"
#include"datatype/Vector3.hpp"

class Object_Light : public GameObject
{
public:

	Object_Light();

	Color LightColor = Color(1.0f, 1.0f, 1.0f);
	float Brightness = 1.0f;
	bool Shadows = false;

	Vector3 Position;

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
	static RegisterDerivedObject<Object_PointLight> RegisterClassAs;
	static void s_DeclareReflections();
	static inline Api s_Api{};
};

class Object_DirectionalLight : public Object_Light
{
public:
	Object_DirectionalLight();

	PHX_GAMEOBJECT_API_REFLECTION;

private:
	static RegisterDerivedObject<Object_DirectionalLight> RegisterClassAs;
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
	static RegisterDerivedObject<Object_SpotLight> RegisterClassAs;
	static void s_DeclareReflections();
	static inline Api s_Api{};
};
