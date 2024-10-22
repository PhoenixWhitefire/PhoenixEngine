#include "gameobject/Primitive.hpp"
#include "asset/MeshProvider.hpp"

static RegisterDerivedObject<Object_Primitive> RegisterClassAs("Primitive");
static bool s_DidInitReflection = false;

static std::string getPrimitiveMesh(PrimitiveShape Type)
{
	switch (Type)
	{

		case PrimitiveShape::Cube:
		{
			return "!Cube";
		}

		default:
		{
			return "!Cube";
		}

	}
}

void Object_Primitive::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
	REFLECTION_INHERITAPI(Object_Base3D);

	REFLECTION_DECLAREPROP(
		"Shape",
		Integer,
		[](GameObject* g)
		{
			return (int)dynamic_cast<Object_Primitive*>(g)->Shape;
		},
		[](GameObject* g, Reflection::GenericValue gv)
		{
			dynamic_cast<Object_Primitive*>(g)->SetShape((PrimitiveShape)gv.AsInteger());
		}
	);
}

Object_Primitive::Object_Primitive()
{
	this->Name = "Primitive";
	this->ClassName = "Primitive";

	this->Material = RenderMaterial::GetMaterial("plastic");
	this->SetShape(PrimitiveShape::Cube);

	s_DeclareReflections();
}

void Object_Primitive::SetShape(PrimitiveShape shape)
{
	this->Shape = shape;
	m_RenderMeshId = MeshProvider::Get()->LoadFromPath(getPrimitiveMesh(shape));
}
