#pragma once

#include"datatype/GameObject.hpp"

#include"datatype/Color.hpp"
#include"datatype/Vector3.hpp"

class Object_Light : public GameObject
{
public:

	Object_Light();

	Color LightColor = Color(1.0f, 1.0f, 1.0f);
	double Brightness = 1.0f;
	bool Shadows = false;

	Vector3 Position;
};

class Object_PointLight : public Object_Light
{
public:

	Object_PointLight();

	double Range = 16.f;

private:
	static DerivedObjectRegister<Object_PointLight> RegisterClassAs;
};

class Object_DirectionalLight : public Object_Light
{
public:
	Object_DirectionalLight();

private:
	static DerivedObjectRegister<Object_DirectionalLight> RegisterClassAs;
};

class Object_SpotLight : public Object_Light
{
public:
	Object_SpotLight();

	double Range = 16.f;

	double OuterCone = 90.f;
	double InnerCone = 95.f;

private:
	static DerivedObjectRegister<Object_SpotLight> RegisterClassAs;
};
