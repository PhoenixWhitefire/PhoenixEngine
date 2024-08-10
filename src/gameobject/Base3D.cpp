#include"gameobject/Base3D.hpp"

static RenderMaterial* DefaultRenderMat = nullptr;

Object_Base3D::Object_Base3D()
{
	this->Name = "Base3D";
	this->ClassName = "Base3D";

	if (!DefaultRenderMat)
		DefaultRenderMat = RenderMaterial::GetMaterial("err");

	this->Material = DefaultRenderMat;

	REFLECTION_DECLAREPROP(
		Position,
		Reflection::ValueType::Vector3,
		[this]()
		{
			Vector3 v = Vector3((glm::vec3)this->Matrix[3]);
			return v;
		},
		[this](Reflection::GenericValue gv)
		{
			Vector3& vec = *(Vector3*)gv.Pointer;
			this->Matrix[3] = glm::vec4(vec.X, vec.Y, vec.Z, 1.f);
		}
	);

	REFLECTION_DECLAREPROP_SIMPLE(Size, Reflection::ValueType::Vector3);

	REFLECTION_DECLAREPROP(
		Material,
		Reflection::ValueType::String,
		[this]()
		{
			return this->Material->Name;
		},
		[this](Reflection::GenericValue gv)
		{
			this->Material = RenderMaterial::GetMaterial(gv.String);
		}
	);

	REFLECTION_DECLAREPROP_SIMPLE(ColorRGB, Reflection::ValueType::Color);
	REFLECTION_DECLAREPROP_SIMPLE(Transparency, Reflection::ValueType::Double);
	REFLECTION_DECLAREPROP_SIMPLE(Reflectivity, Reflection::ValueType::Double);

	REFLECTION_DECLAREPROP(
		FaceCulling,
		Reflection::ValueType::Integer,
		[this]()
		{
			return (int)this->FaceCulling;
		},
		[this](Reflection::GenericValue gv)
		{
			this->FaceCulling = (FaceCullingMode)gv.Integer;
		}
	);
}

Mesh* Object_Base3D::GetRenderMesh()
{
	return &this->RenderMesh;
}
