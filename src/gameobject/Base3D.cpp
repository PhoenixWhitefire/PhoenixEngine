#include<gameobject/Base3D.hpp>

void Object_Base3D::SetRenderMesh(Mesh NewRenderMesh)
{
	this->RenderMesh = NewRenderMesh;
}

Mesh* Object_Base3D::GetRenderMesh()
{
	return &this->RenderMesh;
}
