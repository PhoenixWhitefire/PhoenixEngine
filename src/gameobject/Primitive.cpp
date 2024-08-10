#include"gameobject/Primitive.hpp"
#include"BaseMeshes.hpp"

DerivedObjectRegister<Object_Primitive> Object_Primitive::RegisterClassAs("Primitive");

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

Object_Primitive::Object_Primitive()
{
	this->Name = "Primitive";
	this->ClassName = "Primitive";

	this->RenderMesh = *GetPrimitiveMesh(this->Shape);

	this->Material = RenderMaterial::GetMaterial("plastic");

	REFLECTION_DECLAREPROP(
		Shape,
		Reflection::ValueType::Integer,
		[this]()
		{
			return (int)this->Shape;
		},
		[this](Reflection::GenericValue gv)
		{
			this->SetShape((PrimitiveShape)gv.Integer);
		}
	);
}

void Object_Primitive::SetShape(PrimitiveShape shape)
{
	this->Shape = shape;
	this->RenderMesh = *GetPrimitiveMesh(shape);
}
