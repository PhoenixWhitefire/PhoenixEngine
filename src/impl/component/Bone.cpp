#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_interpolation.hpp>
#include <tracy/Tracy.hpp>

#include "component/Bone.hpp"
#include "component/Mesh.hpp"
#include "datatype/GameObject.hpp"
#include "asset/MeshProvider.hpp"

static EcMesh* getTargetMesh(EcBone* Bone)
{
	if (Bone->TargetMesh != UINT32_MAX)
		return (EcMesh*)GetComponentManagerByComponentType(EntityComponent::Mesh)->GetComponent(Bone->TargetMesh);
	else
		return nullptr;
}

static Bone* getUnderlyingBone(EcBone* BoneComponent)
{
	EcMesh* cm = getTargetMesh(BoneComponent);

	if (cm)
	{
		Mesh& meshData = MeshProvider::Get()->GetMeshResource(cm->RenderMeshId);
		std::vector<Bone>& bones = meshData.Bones;

		if (bones.size() > (size_t)BoneComponent->SkeletalBoneId)
			return &bones[BoneComponent->SkeletalBoneId];
		else
			return nullptr;
	}
	else
		return nullptr;
}

uint32_t BoneComponentManager::CreateComponent(GameObject* Object)
{
	uint32_t id = ComponentManager<EcBone>::CreateComponent(Object);
	m_Components[id].Object = Object;

    return id;
}
	
const Reflection::StaticPropertyMap& BoneComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
		REFLECTION_PROPERTY(
			"Transform",
			Matrix,
			[](void* p)
			-> Reflection::GenericValue
			{
				EcBone* boneObj = static_cast<EcBone*>(p);
				Bone* realBone = getUnderlyingBone(boneObj);
					
				if (realBone)
					return realBone->Transform;
				else
					return boneObj->Transform;
			},
			[](void* p, const Reflection::GenericValue& gv)
			{
                EcBone* boneObj = static_cast<EcBone*>(p);
                boneObj->SetTransform(gv.AsMatrix());

                EcMesh* mesh = getTargetMesh(boneObj);
                mesh->RecomputeBoneMatrices();
			}
		),

		REFLECTION_PROPERTY(
			"IsActive",
			Boolean,
			[](void* p)
			-> Reflection::GenericValue
			{
				return getUnderlyingBone(static_cast<EcBone*>(p)) != nullptr;
			},
			nullptr
		),

		REFLECTION_PROPERTY(
			"SkeletalBoneId",
			Integer,
			[](void* p)
			-> Reflection::GenericValue
			{
				return (uint32_t)static_cast<EcBone*>(p)->SkeletalBoneId;
			},
			nullptr
		),

		REFLECTION_PROPERTY(
			"TargetMesh",
			GameObject,
			[](void* p) -> Reflection::GenericValue
			{
				if (EcMesh* cm = getTargetMesh(static_cast<EcBone*>(p)))
					return cm->Object->ToGenericValue();
				else
					return GameObject::s_ToGenericValue(nullptr);
			},
			nullptr
		)
	};

    return props;
}

void EcBone::SetTransform(const glm::mat4& trans)
{
    Bone* realBone = getUnderlyingBone(this);

    if (realBone)
        realBone->Transform = trans;
    else
    {
        Log.WarningF("Setting transform of unlinked bone {}", Object->GetFullName());
        Transform = trans;
    }
}
