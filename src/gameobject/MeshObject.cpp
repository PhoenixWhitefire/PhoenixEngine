#include"gameobject/MeshObject.hpp"

DerivedObjectRegister<Object_Mesh> Object_Mesh::RegisterClassAs("MeshPart");

Object_Mesh::Object_Mesh()
{
	this->Name = "MeshPart";
	this->ClassName = "MeshPart";
}

void Object_Mesh::SetRenderMesh(Mesh NewRenderMesh)
{
	this->RenderMesh = NewRenderMesh;
}
