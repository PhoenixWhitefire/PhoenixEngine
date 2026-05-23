#include <tracy/Tracy.hpp>

#include "component/Mesh.hpp"
#include "component/Transform.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/MeshProvider.hpp"
#include "component/Workspace.hpp"
#include "component/RigidBody.hpp"
#include "component/Bone.hpp"

static void tryMarkFreeSkinnedMeshPseudoAsset(EcMesh& mesh)
{
	if (Mesh& meshAsset = MeshProvider::Get()->GetMeshResource(mesh.RenderMeshId);
		meshAsset.Bones.size() > 0
	)
	{
		((MeshComponentManager*)MeshComponentManager::Get())->FreeSkinnedMeshPseudoAssets[mesh.Asset].push_back(mesh.RenderMeshId);

		MeshProvider::GpuMesh& gpuMesh = MeshProvider::Get()->GetGpuMesh(meshAsset.GpuId);

		// reset all vertex transformations
		for (glm::mat4& t : gpuMesh.BoneMatrices)
			t = glm::mat4(1.f);
	}
}

uint32_t MeshComponentManager::CreateComponent(GameObject* Object)
{
	uint32_t id = ComponentManager<EcMesh>::CreateComponent(Object);

	EcMesh& cm = m_Components[id];
	cm.MaterialId = MaterialManager::Get()->LoadFromPath("plastic");
	cm.RenderMeshId = MeshProvider::Get()->LoadFromPath("!Cube");
	cm.ComponentId = id;
	cm.Object = Object;

    return id;
}

void MeshComponentManager::DeleteComponent(uint32_t Id)
{
    // TODO id reuse with handles that have a counter per re-use to reduce memory growth

	EcMesh& mesh = m_Components[Id];
	tryMarkFreeSkinnedMeshPseudoAsset(mesh);

	ComponentManager<EcMesh>::DeleteComponent(Id);
}

Reflection::GenericValue MeshComponentManager::GetDefaultPropertyValue(const std::string_view& Property)
{
	return GetDefaultPropertyValue(&GetProperties().at(Property));
}

Reflection::GenericValue MeshComponentManager::GetDefaultPropertyValue(const Reflection::PropertyDescriptor* Property)
{
	static EcMesh Defaults;
	Defaults.MaterialId = MaterialManager::Get()->LoadFromPath("plastic");
	Defaults.RenderMeshId = MeshProvider::Get()->LoadFromPath("!Cube");

	return Property->Get((void*)&Defaults);
}

const Reflection::StaticPropertyMap& MeshComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
		REFLECTION_PROPERTY_SIMPLE(EcMesh, CastsShadows, Boolean),

		REFLECTION_PROPERTY(
			"FaceCulling",
			Integer,
			[](void* p)
			-> Reflection::GenericValue
			{
				return static_cast<uint32_t>(static_cast<EcMesh*>(p)->FaceCulling);
			},
			[](void* p, const Reflection::GenericValue& gv)
			{
				static_cast<EcMesh*>(p)->FaceCulling = static_cast<FaceCullingMode>(gv.AsInteger());
			}
		),

		REFLECTION_PROPERTY_SIMPLE(EcMesh, Transparency, Double),
		REFLECTION_PROPERTY_SIMPLE(EcMesh, MetalnessFactor, Double),
		REFLECTION_PROPERTY_SIMPLE(EcMesh, RoughnessFactor, Double),
		REFLECTION_PROPERTY_SIMPLE_NGV(EcMesh, Tint, Color),

		REFLECTION_PROPERTY(
			"MeshAsset",
			String,
			REFLECTION_PROPERTY_GET_SIMPLE(EcMesh, Asset),
			[](void* p, const Reflection::GenericValue& gv)
			{
				EcMesh* mesh = static_cast<EcMesh*>(p);
				mesh->SetRenderMesh(gv.AsStringView());
			}
		),

		REFLECTION_PROPERTY(
			"Material",
			String,
			[](void* p)
			-> Reflection::GenericValue
			{
				EcMesh* m = static_cast<EcMesh*>(p);
				MaterialManager* mtlManager = MaterialManager::Get();

				return mtlManager->GetMaterialResource(m->MaterialId).Name;
			},
			[](void* p, const Reflection::GenericValue& gv)
			{
				EcMesh* m = static_cast<EcMesh*>(p);
				MaterialManager* mtlManager = MaterialManager::Get();

				m->MaterialId = mtlManager->LoadFromPath(gv.AsStringView());
			}
		)
    };

    return props;
}

void EcMesh::SetRenderMesh(const std::string_view& MeshPath)
{
	if (MeshPath == Asset)
		return;

	if (RenderMeshId != UINT32_MAX)
		tryMarkFreeSkinnedMeshPseudoAsset(*this);

	MeshProvider* meshProvider = MeshProvider::Get();
	ObjectHandle obj = Object;
	std::string meshPathStr = std::string(MeshPath);

	this->RenderMeshId = meshProvider->LoadFromPath(
		std::string(MeshPath),
		true,
		false,
		[obj, meshPathStr, meshProvider](Mesh& mesh)
		{
			ZoneScoped;

			if (mesh.Bones.size() == 0)
				return;

			// TODO
			// due to skinned mesh transforms being associated with ASSETS
			// and NOT the object using them, these need to be separate to
			// allow multiple instances of the same mesh to have different
			// transforms

			uint32_t meshId = UINT32_MAX;

			if (std::vector<uint32_t>& freelist = ((MeshComponentManager*)MeshComponentManager::Get())->FreeSkinnedMeshPseudoAssets[meshPathStr];
				freelist.size() == 0
			)
			{
				std::string pseudoPath = meshPathStr + "!" + std::to_string(obj->ObjectId);
				meshId = meshProvider->Assign(mesh, pseudoPath);
			}
			else
			{
				meshId = freelist[0];
				freelist[0] = freelist[freelist.size() - 1];
				freelist.erase(freelist.begin() + freelist.size() - 1);
			}

			EcMesh* cm = obj->FindComponent<EcMesh>();
			cm->RenderMeshId = meshId;

			for (const ObjectHandle& ch : obj->GetChildren())
				if (ch->FindComponent<EcBone>())
					ch->Destroy();

			Mesh& meshAfter = meshProvider->GetMeshResource(meshId);
			std::unordered_map<std::string_view, ObjectRef> boneNameToObject;

			for (uint8_t boneId = 0; boneId < meshAfter.Bones.size(); boneId++)
			{
				const Bone& b = meshAfter.Bones[boneId];

				ObjectHandle boneObj;
				if (GameObject* g = obj->FindChild(b.Name))
				{
					if (g->FindComponent<EcBone>())
						boneObj = g;
					else
					{
						Log.WarningF(
							"Non-bone with name '{}' under Mesh at {} will be removed for rig bone",
							b.Name, obj->GetFullName()
						);

						g->Destroy();
						boneObj = GameObjectManager::Get()->Create(EntityComponent::Bone);
					}
				}
				else
					boneObj = GameObjectManager::Get()->Create(EntityComponent::Bone);

				EcBone* bone = boneObj->FindComponent<EcBone>();

				ObjectHandle parent;

				if (b.Parent == UINT8_MAX)
					parent = obj;
				else
				{
					const std::string& parentName = meshAfter.Bones[b.Parent].Name;
					const auto& it = boneNameToObject.find(parentName);

					if (it == boneNameToObject.end())
						Log.ErrorF("Bone '{}' has parent '{}', which was not found in the rig", b.Name, parentName);
					else
						parent = it->second;
				}

				boneObj->SetParent(parent);
				boneObj->Name = b.Name;
				boneObj->Serializes = false;

				bone->Transform = b.Transform;
				bone->SkeletalBoneId = boneId;
				bone->TargetMesh = cm->ComponentId;

				boneNameToObject[b.Name] = boneObj.Reference;
			}

            cm->RecomputeBoneMatrices();
		}
	);
	this->Asset = MeshPath;
}

void EcMesh::RecomputeBoneMatrices()
{
	ZoneScoped;

    Mesh& meshData = MeshProvider::Get()->GetMeshResource(RenderMeshId);
    MeshProvider::GpuMesh& gpuMesh = MeshProvider::Get()->GetGpuMesh(meshData.GpuId);

    std::vector<glm::mat4> boneWorldTransforms = std::vector<glm::mat4>(meshData.Bones.size(), glm::mat4(1.f));

    // TODO avoid recursion by sorting bones based on hierarchy
    std::function<void(uint8_t, const glm::mat4&)> process = [&](uint8_t bid, const glm::mat4& parentTrans)
        {
            const Bone& b = meshData.Bones[bid];
            boneWorldTransforms[bid] = parentTrans * b.Transform;
            gpuMesh.BoneMatrices[bid] = boneWorldTransforms[bid] * b.InverseBind;

            for (uint8_t cid = 0; cid < meshData.Bones.size(); cid++)
            {
                if (meshData.Bones[cid].Parent == bid)
                    process(cid, boneWorldTransforms[bid]);
            }
        };

    for (uint8_t bid = 0; bid < meshData.Bones.size(); bid++)
    {
        if (meshData.Bones[bid].Parent == UINT8_MAX)
            process(bid, glm::mat4(1.f));
    }
}
