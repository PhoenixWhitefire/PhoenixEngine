#include"gameobject/MeshObject.hpp"

RegisterDerivedObject<Object_Mesh> Object_Mesh::RegisterClassAs("Mesh");
static bool s_DidInitReflection = false;

void Object_Mesh::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
	REFLECTION_INHERITAPI(Object_Base3D);

	// TODO: asset reloading
	REFLECTION_DECLAREPROP_SIMPLE(Object_Mesh, Asset, String);
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
