#include"gameobject/Model.hpp"

DerivedObjectRegister<Object_Model> Object_Model::RegisterClassAs("Model");

Object_Model::Object_Model()
{
	this->Name = "Model";
	this->ClassName = "Model";
}
