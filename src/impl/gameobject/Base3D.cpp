#include "gameobject/Base3D.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/MeshProvider.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Base3D);

static bool s_DidInitReflection = false;

void Object_Base3D::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROP(
		"Transform",
		Matrix,
		[](Reflection::Reflectable* p)
		{
			return Reflection::GenericValue(dynamic_cast<Object_Base3D*>(p)->Transform);
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			dynamic_cast<Object_Base3D*>(p)->Transform = gv.AsMatrix();
		}
	);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Base3D, Size, Vector3);

	REFLECTION_DECLAREPROP(
		"Material",
		String,
		[](Reflection::Reflectable* g)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			MaterialManager* mtlManager = MaterialManager::Get();

			return mtlManager->GetMaterialResource(p->MaterialId).Name;
		},
		[](Reflection::Reflectable* g, Reflection::GenericValue gv)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			MaterialManager* mtlManager = MaterialManager::Get();

			p->MaterialId = mtlManager->LoadMaterialFromPath(gv.AsString());
		}
	);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Base3D, Tint, Color);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, CastsShadows, Bool);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Base3D, Transparency, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Base3D, MetallnessFactor, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Base3D, RoughnessFactor, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, PhysicsDynamics, Bool);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, PhysicsCollisions, Bool);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, Density, Double);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, Friction, Double);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Base3D, LinearVelocity, Vector3);

	REFLECTION_DECLAREPROP(
		"FaceCulling",
		Integer,
		[](Reflection::Reflectable* g)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			return (int)p->FaceCulling;
		},
		[](Reflection::Reflectable* g, Reflection::GenericValue gv)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			p->FaceCulling = (FaceCullingMode)gv.AsInteger();
		}
	);
}

Object_Base3D::Object_Base3D()
{
	this->Name = "Base3D";
	this->ClassName = "Base3D";

	this->RenderMeshId = MeshProvider::Get()->LoadFromPath("!Cube");
	this->MaterialId = MaterialManager::Get()->LoadMaterialFromPath("plastic");

	s_DeclareReflections();
	ApiPointer = &s_Api;
}
