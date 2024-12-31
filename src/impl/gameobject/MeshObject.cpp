#include "gameobject/MeshObject.hpp"
#include "asset/MeshProvider.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Mesh);

static bool s_DidInitReflection = false;

void Object_Mesh::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(Base3D);

	REFLECTION_DECLAREPROP(
		"Asset",
		String,
		[](Reflection::Reflectable* p)
		{
			return static_cast<Object_Mesh*>(p)->GetRenderMeshPath();
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			static_cast<Object_Mesh*>(p)->SetRenderMesh(gv.AsString());
		}
	);
}

Object_Mesh::Object_Mesh()
{
	this->Name = "Mesh";
	this->ClassName = "Mesh";

	this->SetRenderMesh("!Cube");

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

void Object_Mesh::SetRenderMesh(const std::string& NewRenderMesh)
{
	this->RenderMeshId = MeshProvider::Get()->LoadFromPath(NewRenderMesh);
	m_MeshPath = NewRenderMesh;
}

std::string Object_Mesh::GetRenderMeshPath()
{
	return m_MeshPath;
}
