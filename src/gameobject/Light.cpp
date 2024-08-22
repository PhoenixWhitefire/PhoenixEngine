#include"gameobject/Light.hpp"

RegisterDerivedObject<Object_PointLight> Object_PointLight::RegisterClassAs("PointLight");
RegisterDerivedObject<Object_SpotLight> Object_SpotLight::RegisterClassAs("SpotLight");
RegisterDerivedObject<Object_DirectionalLight> Object_DirectionalLight::RegisterClassAs("DirectionalLight");

bool Object_Light::s_DidInitReflection = false;
bool Object_DirectionalLight::s_DidInitReflection = false;
bool Object_PointLight::s_DidInitReflection = false;
bool Object_SpotLight::s_DidInitReflection = false;

void Object_Light::s_DeclareReflections()
{
	if (s_DidInitReflection)
		//return;
	s_DidInitReflection = true;

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Light, LightColor, Color);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Light, Brightness, Double);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Light, Shadows, Bool);
	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Light, Position, Vector3);

	REFLECTION_INHERITAPI(GameObject);
}

void Object_DirectionalLight::s_DeclareReflections()
{
	if (s_DidInitReflection)
		//return;
	s_DidInitReflection = true;

	// Direction is just Position under-the-hood. Remove the duplicate
	ApiReflection->s_Properties.erase("Position");
	// Redirect it to the Position member
	REFLECTION_DECLAREPROP(
		"Direction",
		Vector3,
		[](Reflection::BaseReflectable* g)
		{
			return dynamic_cast<Object_DirectionalLight*>(g)->Position.ToGenericValue();
		},
		[](Reflection::BaseReflectable* g, Reflection::GenericValue gv)
		{
			Object_DirectionalLight* p = dynamic_cast<Object_DirectionalLight*>(g);
			p->Position = gv;
			// Normalize
			if (p->Position.Magnitude > 1.f)
				p->Position = p->Position / p->Position.Magnitude;
		}
	);

	REFLECTION_INHERITAPI(Object_Light);
	REFLECTION_INHERITAPI(GameObject);
}

void Object_PointLight::s_DeclareReflections()
{
	if (s_DidInitReflection)
		//return;
	s_DidInitReflection = true;

	REFLECTION_DECLAREPROP_SIMPLE(Object_PointLight, Range, Double);

	REFLECTION_INHERITAPI(Object_Light);
	REFLECTION_INHERITAPI(GameObject);
}

void Object_SpotLight::s_DeclareReflections()
{
	if (s_DidInitReflection)
		//return;
	s_DidInitReflection = true;

	REFLECTION_DECLAREPROP_SIMPLE(Object_SpotLight, Range, Double);
	REFLECTION_DECLAREPROP_SIMPLE(Object_SpotLight, OuterCone, Double);
	REFLECTION_DECLAREPROP_SIMPLE(Object_SpotLight, InnerCone, Double);

	REFLECTION_INHERITAPI(Object_Light);
	REFLECTION_INHERITAPI(GameObject);
}

Object_Light::Object_Light()
{
	this->Name = "Light (ABSTRACT)";
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
