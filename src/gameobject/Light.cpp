#include"gameobject/Light.hpp"

DerivedObjectRegister<Object_PointLight> Object_PointLight::RegisterClassAs("PointLight");
DerivedObjectRegister<Object_SpotLight> Object_SpotLight::RegisterClassAs("SpotLight");
DerivedObjectRegister<Object_DirectionalLight> Object_DirectionalLight::RegisterClassAs("DirectionalLight");

Object_Light::Object_Light()
{
	this->Name = "Light (ABSTRACT)";
	this->ClassName = "Light";

	REFLECTION_DECLAREPROP_SIMPLE(LightColor, Reflection::ValueType::Color);
	REFLECTION_DECLAREPROP_SIMPLE(Brightness, Reflection::ValueType::Double);
	REFLECTION_DECLAREPROP_SIMPLE(Shadows, Reflection::ValueType::Bool);
	REFLECTION_DECLAREPROP_SIMPLE(Position, Reflection::ValueType::Vector3);
}

Object_PointLight::Object_PointLight()
{
	this->Name = "PointLight";
	this->ClassName = "PointLight";

	REFLECTION_DECLAREPROP_SIMPLE(Range, Reflection::ValueType::Double);
}

Object_DirectionalLight::Object_DirectionalLight()
{
	this->Name = "DirectionalLight";
	this->ClassName = "DirectionalLight";

	// Direction is just Position under-the-hood. Remove the duplicate
	m_properties.erase("Position");
	// Redirect it to the Position member
	REFLECTION_DECLAREPROP(
		Direction,
		Reflection::ValueType::Vector3,
		[this]()
		{
			return this->Position;
		},
		[this](Reflection::GenericValue gv)
		{
			this->Position = Vector3(gv);
			// Normalize
			if (this->Position.Magnitude > 0.f)
				this->Position = this->Position / this->Position.Magnitude;
		}
	);
}

Object_SpotLight::Object_SpotLight()
{
	this->Name = "SpotLight";
	this->ClassName = "SpotLight";

	REFLECTION_DECLAREPROP_SIMPLE(Range, Reflection::ValueType::Double);
	REFLECTION_DECLAREPROP_SIMPLE(OuterCone, Reflection::ValueType::Double);
	REFLECTION_DECLAREPROP_SIMPLE(InnerCone, Reflection::ValueType::Double);
}
