#include"gameobject/MeshObject.hpp"

DerivedObjectRegister<Object_Mesh> Object_Mesh::RegisterClassAs("Mesh");

Object_Mesh::Object_Mesh()
{
	this->Name = "Mesh";
	this->ClassName = "Mesh";

	m_properties.insert(std::pair(
		"Asset",
		std::pair(PropType::String, std::pair(
			[this]() { return GenericType{PropType::String, this->Asset}; },
			[this](GenericType gt) {this->Asset = gt.String; }
		))
	));
}

void Object_Mesh::SetRenderMesh(Mesh NewRenderMesh)
{
	this->RenderMesh = NewRenderMesh;
}
