#include"gameobject/Model.hpp"

RegisterDerivedObject<Object_Model> Object_Model::RegisterClassAs("Model");

static bool didInitReflection = false;

void Object_Model::s_DeclareReflections()
{
	if (didInitReflection)
		//return;
	didInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
}

Object_Model::Object_Model()
{
	this->Name = "Model";
	this->ClassName = "Model";
	s_DeclareReflections();
}
