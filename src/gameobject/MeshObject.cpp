#include"gameobject/MeshObject.hpp"

RegisterDerivedObject<Object_Mesh> Object_Mesh::RegisterClassAs("Mesh");
bool Object_Mesh::s_DidInitReflection = false;

void Object_Mesh::s_DeclareReflections()
{
	if (s_DidInitReflection)
		//return;
	s_DidInitReflection = true;

	// TODO: asset reloading
	REFLECTION_DECLAREPROP_SIMPLE(Object_Mesh, Asset, String);

	REFLECTION_INHERITAPI(Object_Base3D);
	REFLECTION_INHERITAPI(GameObject);
}

Object_Mesh::Object_Mesh()
{
	this->Name = "Mesh";
	this->ClassName = "Mesh";

	s_DeclareReflections();
}

void Object_Mesh::SetRenderMesh(Mesh NewRenderMesh)
{
	this->RenderMesh = NewRenderMesh;
}
