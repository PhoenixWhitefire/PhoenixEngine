#include "gameobject/Bone.hpp"
#include "gameobject/MeshObject.hpp"

#include "asset/MeshProvider.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Bone);

static bool s_DidInitReflections = false;

static Bone* getUnderlyingBone(Object_Bone* BoneObj)
{
	Object_Mesh* mesh = dynamic_cast<Object_Mesh*>(BoneObj->GetParent());

	if (mesh)
	{
		Mesh& meshData = MeshProvider::Get()->GetMeshResource(mesh->RenderMeshId);
		std::vector<Bone>& bones = meshData.Bones;

		if (bones.size() > (size_t)BoneObj->SkeletalBoneId + 1ull)
			return &bones.at(BoneObj->SkeletalBoneId);
		else
			return nullptr;
	}
	else
		return nullptr;
}

void Object_Bone::s_DeclareReflections()
{
	if (s_DidInitReflections)
		return;
	s_DidInitReflections = true;

	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROP_SIMPLE_READONLY(Object_Bone, SkeletalBoneId, Integer);

	REFLECTION_DECLAREPROP(
		"LocalTransform",
		Matrix,
		[](Reflection::Reflectable* p)
		{
			Object_Bone* boneObj = static_cast<Object_Bone*>(p);
			Bone* realBone = getUnderlyingBone(boneObj);

			if (realBone)
				return realBone->Transform;
			else
				return boneObj->LocalTransform;
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			Object_Bone* boneObj = static_cast<Object_Bone*>(p);
			Bone* realBone = getUnderlyingBone(boneObj);

			if (realBone)
				realBone->Transform = gv.AsMatrix();
			else
				boneObj->LocalTransform = gv.AsMatrix();
		}
	);

	REFLECTION_DECLAREPROP(
		"IsActive",
		Bool,
		[](Reflection::Reflectable* p)
		{
			return (getUnderlyingBone(static_cast<Object_Bone*>(p))) != nullptr;
		},
		nullptr
	);
}

Object_Bone::Object_Bone()
{
	Name = "Bone";
	ClassName = "Bone";

	s_DeclareReflections();
	ApiPointer = &s_Api;
}
