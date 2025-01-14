#include "gameobject/MeshObject.hpp"

#include "asset/MeshProvider.hpp"
#include "gameobject/Bone.hpp"

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

void Object_Mesh::Update(double)
{
	if (!m_WaitingForMeshToLoad)
		return;

	MeshProvider* meshProvider = MeshProvider::Get();

	const Mesh& mesh = meshProvider->GetMeshResource(this->RenderMeshId);

	if (mesh.GpuId == UINT32_MAX)
		return;

	for (GameObject* ch : this->GetChildren())
		if (ch->ClassName == "Bone")
			ch->Destroy();

	for (uint8_t boneId = 0; boneId < mesh.Bones.size(); boneId++)
	{
		const Bone& b = mesh.Bones[boneId];

		Object_Bone* bone = static_cast<Object_Bone*>(GameObject::Create("Bone"));
		bone->SetParent(this);

		bone->LocalTransform = b.Transform;
		bone->SkeletalBoneId = boneId;
		bone->Name = b.Name;
		bone->Serializes = false;
	}

	m_WaitingForMeshToLoad = false;
}

void Object_Mesh::SetRenderMesh(const std::string& NewRenderMesh)
{
	MeshProvider* meshProvider = MeshProvider::Get();

	this->RenderMeshId = meshProvider->LoadFromPath(NewRenderMesh);
	m_MeshPath = NewRenderMesh;

	// in case it has any bones
	// can't do this in this function because it'll (most likely)
	// load asynchronously
	m_WaitingForMeshToLoad = true;
}

std::string Object_Mesh::GetRenderMeshPath()
{
	return m_MeshPath;
}
