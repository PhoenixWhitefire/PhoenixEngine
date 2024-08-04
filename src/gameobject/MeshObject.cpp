#include"gameobject/MeshObject.hpp"

DerivedObjectRegister<Object_Mesh> Object_Mesh::RegisterClassAs("Mesh");

Object_Mesh::Object_Mesh()
{
	this->Name = "Mesh";
	this->ClassName = "Mesh";

	m_properties.insert(std::pair(
		"Asset",
		PropInfo
		{
		PropType::String,
		PropReflection
		{
			[this]()
			{
				return this->Asset;
			},

			[this](GenericType gt)
			{
				// TODO: asset reloading
				this->Asset = gt.String;
			}
		}
		}
	));
}

void Object_Mesh::SetRenderMesh(Mesh NewRenderMesh)
{
	this->RenderMesh = NewRenderMesh;
}
