#include"gameobject/Light.hpp"

static RegisterDerivedObject<Object_Light> RegisterClassAs("Light");
RegisterDerivedObject<Object_PointLight> Object_PointLight::RegisterClassAs("PointLight");
RegisterDerivedObject<Object_SpotLight> Object_SpotLight::RegisterClassAs("SpotLight");
RegisterDerivedObject<Object_DirectionalLight> Object_DirectionalLight::RegisterClassAs("DirectionalLight");

static bool s_BaseDidInitReflection = false;
static bool s_DirectionalDidInitReflection = false;
static bool s_PointDidInitReflection = false;
static bool s_SpotDidInitReflection = false;

void Object_Light::s_DeclareReflections()
{
	if (s_BaseDidInitReflection)
		return;
	s_BaseDidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Light, LightColor, Color);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Light, Brightness, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Light, Shadows, Bool);
	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Light, Position, Vector3);
}

void Object_DirectionalLight::s_DeclareReflections()
{
	if (s_DirectionalDidInitReflection)
		return;
	s_DirectionalDidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
	REFLECTION_INHERITAPI(Object_Light);

	// Direction is just Position under-the-hood. Remove the duplicate
	s_Api.Properties.erase("Position");

	// Redirect it to the Position member
	REFLECTION_DECLAREPROP(
		"Direction",
		Vector3,
		[](GameObject* g)
		{
			return dynamic_cast<Object_DirectionalLight*>(g)->Position.ToGenericValue();
		},
		[](GameObject* g, const Reflection::GenericValue& gv)
		{
			Object_DirectionalLight* p = dynamic_cast<Object_DirectionalLight*>(g);
			p->Position = gv;
			// Normalize
			if (p->Position.Magnitude > 1.f)
				p->Position = p->Position / p->Position.Magnitude;
		}
	);
}

void Object_PointLight::s_DeclareReflections()
{
	if (s_PointDidInitReflection)
		return;
	s_PointDidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
	REFLECTION_INHERITAPI(Object_Light);

	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_PointLight, Range, Double, float);
}

void Object_SpotLight::s_DeclareReflections()
{
	if (s_SpotDidInitReflection)
		return;
	s_SpotDidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
	REFLECTION_INHERITAPI(Object_Light);

	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_SpotLight, Range, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_SpotLight, OuterCone, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_SpotLight, InnerCone, Double, float);
}

Object_Light::Object_Light()
{
	this->Name = "Light";
	this->ClassName = "Light";

	s_DeclareReflections();
}

Object_DirectionalLight::Object_DirectionalLight()
{
	this->Name = "DirectionalLight";
	this->ClassName = "DirectionalLight";

	s_DeclareReflections();
}

Object_PointLight::Object_PointLight()
{
	this->Name = "PointLight";
	this->ClassName = "PointLight";

	s_DeclareReflections();
}

Object_SpotLight::Object_SpotLight()
{
	this->Name = "SpotLight";
	this->ClassName = "SpotLight";

	s_DeclareReflections();
}
