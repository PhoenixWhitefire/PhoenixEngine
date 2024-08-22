#include"gameobject/Base3D.hpp"

bool Object_Base3D::s_DidInitReflection = false;
static RenderMaterial* DefaultRenderMat = nullptr;

void Object_Base3D::s_DeclareReflections()
{
	if (s_DidInitReflection)
		//return;
	s_DidInitReflection = true;

	REFLECTION_DECLAREPROP(
		"Position",
		Vector3,
		[](Reflection::BaseReflectable* g)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			Vector3 v = Vector3((glm::vec3)p->Matrix[3]);
			return v.ToGenericValue();
		},
		[](Reflection::BaseReflectable* g, Reflection::GenericValue gv)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			Vector3 vec = gv;
			p->Matrix[3] = glm::vec4(vec.X, vec.Y, vec.Z, 1.f);
		}
	);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Base3D, Size, Vector3);

	REFLECTION_DECLAREPROP(
		"Material",
		String,
		[](Reflection::BaseReflectable* g)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			return p->Material->Name;
		},
		[](Reflection::BaseReflectable* g, Reflection::GenericValue gv)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			p->Material = RenderMaterial::GetMaterial(gv.String);
		}
	);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Base3D, ColorRGB, Color);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, Transparency, Double);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, Reflectivity, Double);

	REFLECTION_DECLAREPROP(
		"FaceCulling",
		Integer,
		[](Reflection::BaseReflectable* g)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			return (int)p->FaceCulling;
		},
		[](Reflection::BaseReflectable* g, Reflection::GenericValue gv)
		{
			Object_Base3D* p = dynamic_cast<Object_Base3D*>(g);
			p->FaceCulling = (FaceCullingMode)gv.Integer;
		}
	);

	REFLECTION_INHERITAPI(GameObject);
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

Mesh* Object_Base3D::GetRenderMesh()
{
	return &this->RenderMesh;
}
