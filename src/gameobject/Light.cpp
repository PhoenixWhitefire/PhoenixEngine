#include<gameobject/Light.hpp>

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
			[this]() { return this->GetColor(); },
			[this](GenericType gt) { this->SetColor(gt.Color3); }
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
			[this]() { return this->GetBrightness(); },
			[this](GenericType gt) { this->SetBrightness(gt.Double); }
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
			[this]() { return this->GetShadowsEnabled(); },
			[this](GenericType gt) { this->SetShadowsEnabled(gt.Bool); }
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
			[this]() { return this->GetPosition(); },
			[this](GenericType gt) { this->SetPosition(gt.Vector3); }
		}
		}
	));
}

GenericType Object_Light::GetColor() const
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

GenericType Object_Light::GetPosition() const
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
		"Range",
		PropInfo
		{
			PropType::Double,
			PropReflection
		{
			[this]() { return this->GetRange(); },
			[this](GenericType gt) { this->SetRange(gt.Double); }
		}
		}
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
			[this]() { return this->GetPosition(); },
			[this](GenericType gt) { this->SetPosition(gt.Vector3); }
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
			[this]() { return this->GetRange(); },
			[this](GenericType gt) { this->SetRange(gt.Double); }
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
			[this]() { return this->GetOuterCone(); },
			[this](GenericType gt) { this->SetOuterCone(gt.Double); }
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
			[this]() { return this->GetInnerCone(); },
			[this](GenericType gt) { this->SetInnerCone(gt.Double); }
		}
		}
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
