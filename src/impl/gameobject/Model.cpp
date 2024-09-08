#include"gameobject/Model.hpp"

RegisterDerivedObject<Object_Model> Object_Model::RegisterClassAs("Model");

static bool s_DidInitReflection = false;

void Object_Model::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
}

Object_Model::Object_Model()
{
	this->Name = "Model";
	this->ClassName = "Model";
	s_DeclareReflections();
}
