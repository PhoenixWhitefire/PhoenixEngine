#include <glm/gtc/matrix_transform.hpp>

#include "gameobject/Light.hpp"
#include "datatype/Vector3.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Light);
PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(DirectionalLight);
PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(PointLight);
PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(SpotLight);

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
	REFLECTION_INHERITAPI(Attachment);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Light, LightColor, Color);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Light, Brightness, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Light, Shadows, Bool);
}

void Object_DirectionalLight::s_DeclareReflections()
{
	if (s_DirectionalDidInitReflection)
		return;
	s_DirectionalDidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
	// no point inheriting from `Attachment` like w/ the other Lights
	// 25/10/2024
	REFLECTION_INHERITAPI(Light);

	s_Api.Properties.erase("LocalTransform");

	REFLECTION_DECLAREPROP(
		"Direction",
		Vector3,
		[](GameObject* g)
		{
			Object_DirectionalLight* dl = dynamic_cast<Object_DirectionalLight*>(g);
			glm::vec3 forward = dl->LocalTransform[3];
			return Vector3(forward).ToGenericValue();
		},
		[](GameObject* g, const Reflection::GenericValue& gv)
		{
			Object_DirectionalLight* p = dynamic_cast<Object_DirectionalLight*>(g);

			Vector3 newDirection{ gv };
			newDirection = newDirection / newDirection.Magnitude();

			p->LocalTransform = glm::translate(glm::mat4(1.f), (glm::vec3)newDirection);
		}
	);
	
}

void Object_PointLight::s_DeclareReflections()
{
	if (s_PointDidInitReflection)
		return;
	s_PointDidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
	REFLECTION_INHERITAPI(Attachment);
	REFLECTION_INHERITAPI(Light);

	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_PointLight, Range, Double, float);
}

void Object_SpotLight::s_DeclareReflections()
{
	if (s_SpotDidInitReflection)
		return;
	s_SpotDidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
	REFLECTION_INHERITAPI(Attachment);
	REFLECTION_INHERITAPI(Light);

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
