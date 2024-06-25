#pragma once

#include"datatype/GameObject.hpp"

#include"datatype/Color.hpp"
#include"datatype/Vector3.hpp"

class Object_Light : public GameObject
{
public:

	Object_Light();

	GenericType GetColor();
	GenericType GetBrightness();
	GenericType GetShadowsEnabled();
	GenericType GetPosition();

	void SetColor(Color);
	void SetBrightness(float);
	void SetShadowsEnabled(bool);
	void SetPosition(Vector3);

	Color LightColor = Color(1.0f, 1.0f, 1.0f);
	float Brightness = 1.0f;
	bool Shadows = false;

	Vector3 Position;
};

class Object_PointLight : public Object_Light
{
public:

	Object_PointLight();

	GenericType GetRange();
	void SetRange(float);

	float Range = 16.f;

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

	GenericType GetRange();
	GenericType GetOuterCone();
	GenericType GetInnerCone();

	void SetRange(float);
	void SetOuterCone(float);
	void SetInnerCone(float);

	float Range = 16.f;

	float OuterCone = 90.f;
	float InnerCone = 95.f;

private:
	static DerivedObjectRegister<Object_SpotLight> RegisterClassAs;
};
