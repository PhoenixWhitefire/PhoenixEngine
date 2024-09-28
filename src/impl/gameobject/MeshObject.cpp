#include"gameobject/MeshObject.hpp"
#include"asset/MeshProvider.hpp"

static RegisterDerivedObject<Object_Mesh> RegisterClassAs("Mesh");

static bool s_DidInitReflection = false;

void Object_Mesh::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
	REFLECTION_INHERITAPI(Object_Base3D);

	REFLECTION_DECLAREPROP(
		"Asset",
		String,
		[](GameObject* p)
		{
			return dynamic_cast<Object_Mesh*>(p)->GetRenderMeshPath();
		},
		[](GameObject* p, const Reflection::GenericValue& gv)
		{
			dynamic_cast<Object_Mesh*>(p)->SetRenderMesh(gv.AsString());
		}
	);
}

Object_Mesh::Object_Mesh()
{
	this->Name = "Mesh";
	this->ClassName = "Mesh";

	s_DeclareReflections();

	this->SetRenderMesh("!Cube");
}

void Object_Mesh::SetRenderMesh(const std::string& NewRenderMesh)
{
	m_RenderMeshId = MeshProvider::Get()->LoadFromPath(NewRenderMesh);
	m_MeshPath = NewRenderMesh;
}

std::string Object_Mesh::GetRenderMeshPath()
{
	return m_MeshPath;
}
