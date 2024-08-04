#include"gameobject/Light.hpp"

DerivedObjectRegister<Object_PointLight> Object_PointLight::RegisterClassAs("PointLight");
DerivedObjectRegister<Object_SpotLight> Object_SpotLight::RegisterClassAs("SpotLight");
DerivedObjectRegister<Object_DirectionalLight> Object_DirectionalLight::RegisterClassAs("DirectionalLight");

Object_Light::Object_Light()
{
	this->Name = "Light (ABSTRACT)";
	this->ClassName = "Light";

	m_properties.insert(std::pair(
		"Color",
		PropInfo
		{
			PropType::Color,
			PropReflection
		{
			[this]() { return this->LightColor; },
			[this](GenericType gt) { this->LightColor = gt; }
		}
		}
	));
	m_properties.insert(std::pair(
		"Brightness",
		PropInfo
		{
			PropType::Double,
			PropReflection
		{
			[this]() { return this->Brightness; },
			[this](GenericType gt) { this->Brightness = (double)gt; }
		}
		}
	));
	m_properties.insert(std::pair(
		"Shadows",
		PropInfo
		{
			PropType::Bool,
			PropReflection
		{
			[this]() { return this->Shadows; },
			[this](GenericType gt) { this->Shadows = gt; }
		}
		}
	));
	m_properties.insert(std::pair(
		"Position",
		PropInfo
		{
			PropType::Vector3,
			PropReflection
		{
			[this]() { return this->Position; },
			[this](GenericType gt) { this->Position = gt; }
		}
		}
	));
}

Object_PointLight::Object_PointLight()
{
	this->Name = "PointLight";
	this->ClassName = "PointLight";

	m_properties.insert(std::pair(
		"Range",
		PropInfo
		{
			PropType::Double,
			PropReflection
		{
			[this]() { return this->Range; },
			[this](GenericType gt) { this->Range = (double)gt; }
		}
		}
	));
}

Object_DirectionalLight::Object_DirectionalLight()
{
	this->Name = "DirectionalLight";
	this->ClassName = "DirectionalLight";

	// Direction is just Position under-the-hood. Remove the duplicate
	m_properties.erase("Position");
	// Redirect it to the Position member
	m_properties.insert(std::pair(
		"Direction",
		PropInfo
		{
			PropType::Vector3,
			PropReflection
		{
			[this]() { return this->Position; },
			[this](GenericType gt) { this->Position = gt; }
		}
		}
	));
}

Object_SpotLight::Object_SpotLight()
{
	this->Name = "SpotLight";
	this->ClassName = "SpotLight";

	m_properties.insert(std::pair(
		"Range",
		PropInfo
		{
			PropType::Double,
			PropReflection
		{
			[this]() { return this->Range; },
			[this](GenericType gt) { this->Range = (double)gt; }
		}
		}
	));

	m_properties.insert(std::pair(
		"OuterCone",
		PropInfo
		{
			PropType::Double,
			PropReflection
		{
			[this]() { return this->OuterCone; },
			[this](GenericType gt) { this->OuterCone = (double)gt; }
		}
		}
	));

	m_properties.insert(std::pair(
		"InnerCone",
		PropInfo
		{
			PropType::Double,
			PropReflection
		{
			[this]() { return this->InnerCone; },
			[this](GenericType gt) { this->InnerCone = (double)gt; }
		}
		}
	));
}
