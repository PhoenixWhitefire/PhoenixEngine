#include<gameobject/Light.hpp>

DerivedObjectRegister<Object_PointLight> Object_PointLight::RegisterClassAs("PointLight");
DerivedObjectRegister<Object_SpotLight> Object_SpotLight::RegisterClassAs("SpotLight");
DerivedObjectRegister<Object_DirectionalLight> Object_DirectionalLight::RegisterClassAs("DirectionalLight");

Object_Light::Object_Light()
{
	this->Name = "BaseLight";
	this->ClassName = "Light";

	m_properties.insert(std::pair(
		std::string("Color"),
		std::pair(PropType::Color, std::pair(
			[this]() { return this->GetColor(); },
			[this](GenericType gt) { this->SetColor(gt.Color3); }
		))
	));
	m_properties.insert(std::pair(
		std::string("Brightness"),
		std::pair(PropType::Double, std::pair(
			[this]() { return this->GetBrightness(); },
			[this](GenericType gt) { this->SetBrightness(gt.Double); }
		))
	));
	m_properties.insert(std::pair(
		std::string("Shadows"),
		std::pair(PropType::Bool, std::pair(
			[this]() { return this->GetShadowsEnabled(); },
			[this](GenericType gt) { this->SetShadowsEnabled(gt.Bool); }
		))
	));
	m_properties.insert(std::pair(
		std::string("Position"),
		std::pair(PropType::Vector3, std::pair(
			[this]() { return this->GetPosition(); },
			[this](GenericType gt) { this->SetPosition(gt.Vector3); }
		))
	));
}

GenericType Object_Light::GetColor()
{
	GenericType gt;
	gt.Type = PropType::Color;
	gt.Color3 = this->LightColor;
	return gt;
}

GenericType Object_Light::GetBrightness()
{
	return
	{
		PropType::Double,
		"",
		false,
		this->Brightness
	};
}

GenericType Object_Light::GetShadowsEnabled()
{
	return
	{
		PropType::Bool,
		"",
		this->Shadows
	};
}

GenericType Object_Light::GetPosition()
{
	GenericType gt;
	gt.Type = PropType::Vector3;
	gt.Vector3 = this->Position;
	return gt;
}

void Object_Light::SetColor(Color col)
{
	this->LightColor = col;
}

void Object_Light::SetBrightness(float br)
{
	this->Brightness = br;
}

void Object_Light::SetShadowsEnabled(bool b)
{
	this->Shadows = b;
}

void Object_Light::SetPosition(Vector3 v3)
{
	this->Position = v3;
}

Object_PointLight::Object_PointLight()
{
	this->Name = "PointLight";
	this->ClassName = "PointLight";

	m_properties.insert(std::pair(
		std::string("Range"),
		std::pair(PropType::Double, std::pair(
			[this]() { return this->GetRange(); },
			[this](GenericType gt) { this->SetRange(gt.Double); }
		))
	));
}

GenericType Object_PointLight::GetRange()
{
	return
	{
		PropType::Double,
		"",
		false,
		this->Range
	};
}

void Object_PointLight::SetRange(float r)
{
	this->Range = r;
}

Object_DirectionalLight::Object_DirectionalLight()
{
	this->Name = "DirectionalLight";
	this->ClassName = "DirectionalLight";
}

Object_SpotLight::Object_SpotLight()
{
	this->Name = "SpotLight";
	this->ClassName = "SpotLight";

	m_properties.insert(std::pair(
		std::string("Range"),
		std::pair(PropType::Double, std::pair(
			[this]() { return this->GetRange(); },
			[this](GenericType gt) { this->SetRange(gt.Double); }
		))
	));

	m_properties.insert(std::pair(
		std::string("OuterCone"),
		std::pair(PropType::Double, std::pair(
			[this]() { return this->GetOuterCone(); },
			[this](GenericType gt) { this->SetOuterCone(gt.Double); }
		))
	));

	m_properties.insert(std::pair(
		std::string("InnerCone"),
		std::pair(PropType::Double, std::pair(
			[this]() { return this->GetInnerCone(); },
			[this](GenericType gt) { this->SetInnerCone(gt.Double); }
		))
	));
}

GenericType Object_SpotLight::GetRange()
{
	return
	{
		PropType::Double,
		"",
		false,
		this->Range
	};
}

GenericType Object_SpotLight::GetOuterCone()
{
	return
	{
		PropType::Double,
		"",
		false,
		this->Range
	};
}

GenericType Object_SpotLight::GetInnerCone()
{
	return
	{
		PropType::Double,
		"",
		false,
		this->Range
	};
}

void Object_SpotLight::SetRange(float r)
{
	this->Range = r;
}

void Object_SpotLight::SetOuterCone(float o)
{
	this->OuterCone = o;
}

void Object_SpotLight::SetInnerCone(float i)
{
	this->InnerCone = i;
}
