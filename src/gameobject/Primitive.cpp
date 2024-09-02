#include"gameobject/Primitive.hpp"
#include"BaseMeshes.hpp"

RegisterDerivedObject<Object_Primitive> Object_Primitive::RegisterClassAs("Primitive");
static bool s_DidInitReflection = false;

static Mesh* GetPrimitiveMesh(PrimitiveShape Type)
{
	Mesh* PrimitiveMesh;

	switch (Type)
	{

		case PrimitiveShape::Cube:
		{
			PrimitiveMesh = BaseMeshes::Cube();
			break;
		}

		default:
		{
			PrimitiveMesh = BaseMeshes::Cube();
			break;
		}

	}

	return PrimitiveMesh;
}

void Object_Primitive::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
	REFLECTION_INHERITAPI(Base3D);

	REFLECTION_DECLAREPROP(
		"Shape",
		Integer,
		[](GameObject* g)
		{
			return (int)dynamic_cast<Object_Primitive*>(g)->Shape;
		},
		[](GameObject* g, Reflection::GenericValue gv)
		{
			dynamic_cast<Object_Primitive*>(g)->SetShape((PrimitiveShape)gv.Integer);
		}
	);
}

Object_Primitive::Object_Primitive()
{
	this->Name = "Primitive";
	this->ClassName = "Primitive";

	this->RenderMesh = *GetPrimitiveMesh(this->Shape);

	this->Material = RenderMaterial::GetMaterial("plastic");

	s_DeclareReflections();
}

void Object_Primitive::SetShape(PrimitiveShape shape)
{
	this->Shape = shape;
	this->RenderMesh = *GetPrimitiveMesh(shape);
}
