#include "gameobject/Primitive.hpp"
#include "asset/MeshProvider.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Primitive);

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

	REFLECTION_INHERITAPI(Base3D);

	REFLECTION_DECLAREPROP(
		"Shape",
		Integer,
		[](Reflection::Reflectable* g)
		{
			return (int)static_cast<Object_Primitive*>(g)->Shape;
		},
		[](Reflection::Reflectable* g, Reflection::GenericValue gv)
		{
			static_cast<Object_Primitive*>(g)->SetShape((PrimitiveShape)gv.AsInteger());
		}
	);
}

Object_Primitive::Object_Primitive()
{
	this->Name = "Primitive";
	this->ClassName = "Primitive";

	this->SetShape(PrimitiveShape::Cube);

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

void Object_Primitive::SetShape(PrimitiveShape shape)
{
	this->Shape = shape;
	this->RenderMeshId = MeshProvider::Get()->LoadFromPath(getPrimitiveMesh(shape));
}
