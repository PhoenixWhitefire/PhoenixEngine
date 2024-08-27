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

private:
	
	static void s_DeclareReflections();
	static bool s_DidInitReflection;
};

class Object_PointLight : public Object_Light
{
public:

	Object_PointLight();

	float Range = 16.f;

private:
	static RegisterDerivedObject<Object_PointLight> RegisterClassAs;
	static void s_DeclareReflections();
	static bool s_DidInitReflection;
};

class Object_DirectionalLight : public Object_Light
{
public:
	Object_DirectionalLight();

private:
	static RegisterDerivedObject<Object_DirectionalLight> RegisterClassAs;
	static void s_DeclareReflections();
	static bool s_DidInitReflection;
};

class Object_SpotLight : public Object_Light
{
public:
	Object_SpotLight();

	float Range = 16.f;

	float OuterCone = 90.f;
	float InnerCone = 95.f;

private:
	static RegisterDerivedObject<Object_SpotLight> RegisterClassAs;
	static void s_DeclareReflections();
	static bool s_DidInitReflection;
};
