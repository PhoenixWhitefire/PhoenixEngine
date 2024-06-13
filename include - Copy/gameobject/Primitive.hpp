#pragma once

#include<datatype/GameObject.hpp>
#include<gameobject/Base3D.hpp>

enum class PrimitiveShape { Cube };

class Object_Primitive : public Object_Base3D
{
public:
	Object_Primitive();

	void SetShape(PrimitiveShape);

	std::string Name = "Primitive";
	std::string ClassName = "Primitive";

	PrimitiveShape Shape = PrimitiveShape::Cube;

private:
	static DerivedObjectRegister<Object_Primitive> RegisterClassAs;
};
