#include "gameobject/Base3D.hpp"
#include "asset/MaterialManager.hpp"

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
		[](GameObject* p)
		{
			return Reflection::GenericValue(dynamic_cast<Object_Base3D*>(p)->Transform);
		},
		[](GameObject* p, const Reflection::GenericValue& gv)
		{
			dynamic_cast<Object_Base3D*>(p)->Transform = gv.AsMatrix();
		}
	);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Base3D, Size, Vector3);

	REFLECTION_DECLAREPROP(
		"Material",
		String,
		[](GameObject* g)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			MaterialManager* mtlManager = MaterialManager::Get();

			return mtlManager->GetMaterialResource(p->MaterialId).Name;
		},
		[](GameObject* g, Reflection::GenericValue gv)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			MaterialManager* mtlManager = MaterialManager::Get();

			p->MaterialId = mtlManager->LoadMaterialFromPath(gv.AsString());
		}
	);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Base3D, ColorRGB, Color);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Base3D, Transparency, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Base3D, Reflectivity, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, PhysicsDynamics, Bool);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, PhysicsCollisions, Bool);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, Density, Double);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, Friction, Double);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Base3D, LinearVelocity, Vector3);

	REFLECTION_DECLAREPROP(
		"FaceCulling",
		Integer,
		[](GameObject* g)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			return (int)p->FaceCulling;
		},
		[](GameObject* g, Reflection::GenericValue gv)
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

	this->MaterialId = MaterialManager::Get()->LoadMaterialFromPath("plastic");

	s_DeclareReflections();
}

uint32_t Object_Base3D::GetRenderMeshId()
{
	return m_RenderMeshId;
}
