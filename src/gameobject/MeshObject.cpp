#include"gameobject/MeshObject.hpp"

DerivedObjectRegister<Object_Mesh> Object_Mesh::RegisterClassAs("Mesh");

Object_Mesh::Object_Mesh()
{
	this->Name = "Mesh";
	this->ClassName = "Mesh";
}

void Object_Mesh::SetRenderMesh(Mesh NewRenderMesh)
{
	this->RenderMesh = NewRenderMesh;
}
