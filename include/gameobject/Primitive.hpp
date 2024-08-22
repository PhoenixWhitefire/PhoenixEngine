#pragma once

#include<datatype/GameObject.hpp>
#include<gameobject/Base3D.hpp>

enum class PrimitiveShape { Cube };

class Object_Primitive : public Object_Base3D
{
public:
	Object_Primitive();

	void SetShape(PrimitiveShape);

	PrimitiveShape Shape = PrimitiveShape::Cube;

private:
	static RegisterDerivedObject<Object_Primitive> RegisterClassAs;
	static void s_DeclareReflections();
	static bool s_DidInitReflection;
};
