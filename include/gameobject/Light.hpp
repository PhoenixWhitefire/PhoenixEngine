#pragma once

#include<datatype/GameObject.hpp>

#include<datatype/Color.hpp>
#include<datatype/Vector3.hpp>

class Object_Light : public GameObject
{
public:
	std::string Name = "Pointlight";
	std::string ClassName = "PointLight";

	Color LightColor = Color(1.0f, 1.0f, 1.0f);
	float Brightness = 1.0f;
	bool Shadows = false;

	Vector3 Position;
};

class Object_PointLight : public Object_Light
{
public:
	std::string ClassName = "PointLight";

	float Range = 16.f;

private:
	static DerivedObjectRegister<Object_PointLight> RegisterClassAs;
};

class Object_DirectionalLight : public Object_Light
{
public:
	std::string ClassName = "DirectionalLight";

private:
	static DerivedObjectRegister<Object_DirectionalLight> RegisterClassAs;
};

class Object_SpotLight : public Object_Light
{
public:
	std::string ClassName = "SpotLight";

	float Range = 16.f;

	float OuterCone = 90.f;
	float InnerCone = 95.f;

private:
	static DerivedObjectRegister<Object_SpotLight> RegisterClassAs;
};
