#include"gameobject/Base3D.hpp"

static bool s_DidInitReflection = false;
static RenderMaterial* DefaultRenderMat = nullptr;

RegisterDerivedObject<Object_Base3D> RegisterClassAs("Base3D");

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
			return p->Material->Name;
		},
		[](GameObject* g, Reflection::GenericValue gv)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			p->Material = RenderMaterial::GetMaterial(gv.AsString());
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

	if (!DefaultRenderMat)
		DefaultRenderMat = RenderMaterial::GetMaterial("err");

	this->Material = DefaultRenderMat;

	s_DeclareReflections();
}

uint32_t Object_Base3D::GetRenderMeshId()
{
	return m_RenderMeshId;
}
