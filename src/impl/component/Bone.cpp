#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_interpolation.hpp>

#include "component/Bone.hpp"
#include "component/Mesh.hpp"
#include "datatype/GameObject.hpp"
#include "asset/MeshProvider.hpp"

static Bone* getUnderlyingBone(EcBone* BoneComponent)
{
	GameObject* meshObj = BoneComponent->Object->GetParent();
	EcMesh* mesh = meshObj ? meshObj->FindComponent<EcMesh>() : nullptr;

	if (mesh)
	{
		Mesh& meshData = MeshProvider::Get()->GetMeshResource(mesh->RenderMeshId);
		std::vector<Bone>& bones = meshData.Bones;

		if (bones.size() >= (size_t)BoneComponent->SkeletalBoneId)
			return &bones.at(BoneComponent->SkeletalBoneId);
		else
			return nullptr;
	}
	else
		return nullptr;
}

class BoneManager : public ComponentManager<EcBone>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
		m_Components.back().Object = Object;

        return static_cast<uint32_t>(m_Components.size() - 1);
    }
	
    const Reflection::StaticPropertyMap& GetProperties() override
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
					Bone* realBone = getUnderlyingBone(boneObj);
					
					if (realBone)
					{
						ZoneNamed("UpdateSkinningTransforms");

						realBone->Transform = gv.AsMatrix();

						GameObject* meshObj = boneObj->Object->GetParent();
						EcMesh* mesh = meshObj->FindComponent<EcMesh>();

						Mesh& meshData = MeshProvider::Get()->GetMeshResource(mesh->RenderMeshId);
						MeshProvider::GpuMesh& gpuMesh = MeshProvider::Get()->GetGpuMesh(meshData.GpuId);

						glm::mat4 accumulatedTransform = { 1.f };
						uint8_t parent = realBone->Parent;

						while (parent != UINT8_MAX)
						{
							const Bone& parentBone = meshData.Bones[parent];
							accumulatedTransform = parentBone.Transform * accumulatedTransform;
							parent = parentBone.Parent;
						}

						for (auto [vi, ji] : realBone->TargetVertices)
						{
							const Vertex& v = meshData.Vertices[vi];
							gpuMesh.SkinningData[vi] = accumulatedTransform * glm::interpolate(glm::mat4(1.f), realBone->Transform, v.JointWeights[ji]);
						}

						accumulatedTransform *= realBone->Transform;

						for (const Bone& childBone : meshData.Bones)
						{
							if (childBone.Parent != boneObj->SkeletalBoneId)
								continue;

							for (auto [vi, ji] : childBone.TargetVertices)
							{
								const Vertex& v = meshData.Vertices[vi];
								gpuMesh.SkinningData[vi] = accumulatedTransform * glm::interpolate(glm::mat4(1.f), childBone.Transform, v.JointWeights[ji]);
							}
						}
					}
					else
						boneObj->Transform = gv.AsMatrix();
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
			)
		};

        return props;
    }
};

static BoneManager Instance;
