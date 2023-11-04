#include<datatype/GameObject.hpp>
#include<gameobject/Base3D.hpp>

enum class PrimitiveType { Cube };

class Object_Primitive : public Object_Base3D
{
public:
	std::string Name = "Primitive";
	std::string ClassName = "Primitive";

	PrimitiveType Type = PrimitiveType::Cube;

	void Initialize();
	void Update(double DeltaTime);

private:
	static DerivedObjectRegister<Object_Primitive> RegisterClassAs;

	PrimitiveType PreviousType = PrimitiveType::Cube;
};
