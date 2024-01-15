#include<gameobject/Primitive.hpp>
#include<BaseMeshes.hpp>

DerivedObjectRegister<Object_Primitive> Object_Primitive::RegisterClassAs("Primitive");

Mesh* GetPrimitiveMesh(PrimitiveType Type)
{
	Mesh* PrimitiveMesh;

	switch (Type)
	{

		case PrimitiveType::Cube:
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

void Object_Primitive::Initialize()
{
	this->RenderMesh = *GetPrimitiveMesh(this->Type);
	this->PreviousType = this->Type;
}

void Object_Primitive::Update(double DeltaTime)
{
	if (this->Type != this->PreviousType)
	{
		this->RenderMesh = *GetPrimitiveMesh(this->Type);
		this->PreviousType = this->Type;
	}
}
