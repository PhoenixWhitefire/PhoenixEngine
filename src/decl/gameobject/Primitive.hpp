#pragma once

#include <datatype/GameObject.hpp>
#include <gameobject/Base3D.hpp>

enum class PrimitiveShape : uint8_t { Cube };

class Object_Primitive : public Object_Base3D
{
public:
	Object_Primitive();

	void SetShape(PrimitiveShape);

	PrimitiveShape Shape = PrimitiveShape::Cube;

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};
